/*
 *      highligting.c - this file is part of Geany, a fast and lightweight IDE
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

#include <stdlib.h>

#include "SciLexer.h"
#include "geany.h"
#include "highlighting.h"
#include "sci_cb.h"
#include "utils.h"


static style_set *types[GEANY_MAX_FILE_TYPES] = { NULL };
static gboolean global_c_tags_loaded = FALSE;
static gboolean global_php_tags_loaded = FALSE;
static gboolean global_html_tags_loaded = FALSE;
static gboolean global_latex_tags_loaded = FALSE;


/* simple wrapper function to print file errors in DEBUG mode */
static void styleset_load_file(GKeyFile *key_file, const gchar *file, GKeyFileFlags flags,
								G_GNUC_UNUSED GError **just_for_compatibility)
{
	GError *error = NULL;
	gboolean done = g_key_file_load_from_file(key_file, file, flags, &error);
	if (! done && error != NULL)
	{
		geany_debug("Failed to open %s (%s)", file, error->message);
		g_error_free(error);
		error = NULL;
	}
}


static void styleset_get_keywords(GKeyFile *config, GKeyFile *configh, const gchar *section,
								  const gchar *key, gint index, gint pos, const gchar *default_value)
{
	gchar *result;

	if (config == NULL || configh == NULL || section == NULL)
	{
		types[index]->keywords[pos] = g_strdup(default_value);
		return;
	}

	result = g_key_file_get_string(configh, section, key, NULL);
	if (result == NULL) result = g_key_file_get_string(config, section, key, NULL);

	if (result == NULL)
	{
		types[index]->keywords[pos] = g_strdup(default_value);
	}
	else
	{
		types[index]->keywords[pos] = result;
	}
}


static void styleset_get_wordchars(GKeyFile *config, GKeyFile *configh, gint index, const gchar *default_value)
{
	gchar *result;

	if (config == NULL || configh == NULL)
	{
		types[index]->wordchars = g_strdup(default_value);
		return;
	}

	result = g_key_file_get_string(configh, "settings", "wordchars", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "settings", "wordchars", NULL);

	if (result == NULL)
	{
		types[index]->wordchars = g_strdup(default_value);
	}
	else
		types[index]->wordchars = result;
}


static void styleset_get_hex(GKeyFile *config, GKeyFile *configh, const gchar *section, const gchar *key,
					  const gchar *foreground, const gchar *background, const gchar *bold, gint array[])
{
	gchar **list;
	gsize len;

	if (config == NULL || configh == NULL || section == NULL) return;

	list = g_key_file_get_string_list(configh, section, key, &len, NULL);
	if (list == NULL) list = g_key_file_get_string_list(config, section, key, &len, NULL);
	if (list != NULL && list[0] != NULL) array[0] = (gint) utils_strtod(list[0], NULL, FALSE);
	else if (foreground) array[0] = (gint) utils_strtod(foreground, NULL, FALSE);
	if (list && list != NULL && list[1] != NULL) array[1] = (gint) utils_strtod(list[1], NULL, FALSE);
	else if (background) array[1] = (gint) utils_strtod(background, NULL, FALSE);
	if (list != NULL && list[2] != NULL) array[2] = utils_atob(list[2]);
	else array[2] = utils_atob(bold);
	if (list != NULL && list[3] != NULL) array[3] = utils_atob(list[3]);
	else array[3] = FALSE;

	g_strfreev(list);
}


static void styleset_get_int(GKeyFile *config, GKeyFile *configh, const gchar *section,
							 const gchar *key, gint fdefault_val, gint sdefault_val, gint array[])
{
	gchar **list;
	gchar *end1, *end2;
	gsize len;

	if (config == NULL || configh == NULL || section == NULL) return;

	list = g_key_file_get_string_list(configh, section, key, &len, NULL);
	if (list == NULL) list = g_key_file_get_string_list(config, section, key, &len, NULL);

	if (list != NULL && list[0] != NULL) array[0] = strtol(list[0], &end1, 10);
	else array[0] = fdefault_val;
	if (list != NULL && list[1] != NULL) array[1] = strtol(list[1], &end2, 10);
	else array[1] = sdefault_val;

	// if there was an error, strtol() returns 0 and end is list[x], so then we use default_val
	if (list == NULL || list[0] == end1) array[0] = fdefault_val;
	if (list == NULL || list[1] == end2) array[1] = sdefault_val;

	g_strfreev(list);
}

static guint invert(guint icolour)
{
	if (types[GEANY_FILETYPES_ALL]->styling[11][0])
	{
		guint r, g, b;

		r = 0xffffff - icolour;
		g = 0xffffff - (icolour >> 8);
		b = 0xffffff - (icolour >> 16);
		return (r | (g << 8) | (b << 16));
	}
	return icolour;
}


static void styleset_set_style(ScintillaObject *sci, gint style, gint filetype, gint styling_index)
{
	SSM(sci, SCI_STYLESETFORE, style, invert(types[filetype]->styling[styling_index][0]));
	SSM(sci, SCI_STYLESETBACK, style, invert(types[filetype]->styling[styling_index][1]));
	SSM(sci, SCI_STYLESETBOLD, style, types[filetype]->styling[styling_index][2]);
	SSM(sci, SCI_STYLESETITALIC, style, types[filetype]->styling[styling_index][3]);
}


void styleset_free_styles()
{
	gint i;

	for (i = 0; i < GEANY_MAX_FILE_TYPES; i++)
	{
		if (types[i] != NULL)
		{
			g_strfreev(types[i]->keywords);
			g_free(types[i]->wordchars);
			g_free(types[i]);
		}
	}
}


static void styleset_common_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.common", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.common", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_ALL] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_ALL]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "selection", "0xc0c0c0", "0x7f0000", "false", types[GEANY_FILETYPES_ALL]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "brace_good", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_ALL]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "brace_bad", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_ALL]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "margin_linenumber", "0x000000", "0xd0d0d0", "false", types[GEANY_FILETYPES_ALL]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "margin_folding", "0x000000", "0xdfdfdf", "false", types[GEANY_FILETYPES_ALL]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "current_line", "0x000000", "0xe5e5e5", "true", types[GEANY_FILETYPES_ALL]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "caret", "0x000000", "0x000000", "false", types[GEANY_FILETYPES_ALL]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "indent_guide", "0xc0c0c0", "0xffffff", "false", types[GEANY_FILETYPES_ALL]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "white_space", "0xc0c0c0", "0xffffff", "true", types[GEANY_FILETYPES_ALL]->styling[9]);
	styleset_get_int(config, config_home, "styling", "folding_style", 1, 1, types[GEANY_FILETYPES_ALL]->styling[10]);
	styleset_get_int(config, config_home, "styling", "invert_all", 0, 0, types[GEANY_FILETYPES_ALL]->styling[11]);

	types[GEANY_FILETYPES_ALL]->keywords = NULL;
	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_ALL, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_ALL);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_common(ScintillaObject *sci, gint style_bits)
{
	if (types[GEANY_FILETYPES_ALL] == NULL) styleset_common_init();

	SSM(sci, SCI_STYLECLEARALL, 0, 0);

	SSM(sci, SCI_SETUSETABS, TRUE, 0);

	// caret colour
	SSM(sci, SCI_SETCARETFORE, invert(types[GEANY_FILETYPES_ALL]->styling[7][0]), 0);

	// colourize the current line
	SSM(sci, SCI_SETCARETLINEBACK, invert(types[GEANY_FILETYPES_ALL]->styling[6][1]), 0);
	SSM(sci, SCI_SETCARETLINEVISIBLE, types[GEANY_FILETYPES_ALL]->styling[6][2], 0);

	// indicator settings
	SSM(sci, SCI_INDICSETSTYLE, 2, INDIC_SQUIGGLE);
	// why? if I let this out, the indicator remains green with PHP
	SSM(sci, SCI_INDICSETFORE, 0, invert(0x0000ff));
	SSM(sci, SCI_INDICSETFORE, 2, invert(0x0000ff));

	// define marker symbols
	// 0 -> line marker
	SSM(sci, SCI_MARKERDEFINE, 0, SC_MARK_SHORTARROW);
	SSM(sci, SCI_MARKERSETFORE, 0, invert(0x00007f));
	SSM(sci, SCI_MARKERSETBACK, 0, invert(0x00ffff));

	// 1 -> user marker
	SSM(sci, SCI_MARKERDEFINE, 1, SC_MARK_PLUS);
	SSM(sci, SCI_MARKERSETFORE, 1, invert(0x000000));
	SSM(sci, SCI_MARKERSETBACK, 1, invert(0xB8F4B8));

	// 2 -> folding marker, other folding settings
	SSM(sci, SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);
	SSM(sci, SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);
	SSM(sci, SCI_SETFOLDFLAGS, 0, 0);

	// choose the folding style - boxes or circles, I prefer boxes, so it is default ;-)
	switch (types[GEANY_FILETYPES_ALL]->styling[10][0])
	{
		case 2:
		{
			SSM (sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEROPEN, SC_MARK_CIRCLEMINUS);
			SSM (sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDER, SC_MARK_CIRCLEPLUS);
			SSM (sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEREND, SC_MARK_CIRCLEPLUSCONNECTED);
			SSM (sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEROPENMID, SC_MARK_CIRCLEMINUSCONNECTED);
			break;
		}
		default:
		{
			SSM (sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
			SSM (sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
			SSM (sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED);
			SSM (sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED);
			break;
		}
	}

	// choose the folding style - straight or curved, I prefer straight, so it is default ;-)
	switch (types[GEANY_FILETYPES_ALL]->styling[10][1])
	{
		case 2:
		{
			SSM (sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNERCURVE);
			SSM (sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNERCURVE);
			break;
		}
		default:
		{
			SSM (sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER);
			SSM (sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER);
			break;
		}
	}

	SSM(sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);

	SSM(sci,SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPEN, 0xffffff);
	SSM(sci,SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPEN, 0x000000);
	SSM(sci,SCI_MARKERSETFORE, SC_MARKNUM_FOLDER, 0xffffff);
	SSM(sci,SCI_MARKERSETBACK, SC_MARKNUM_FOLDER, 0x000000);
	SSM(sci,SCI_MARKERSETFORE, SC_MARKNUM_FOLDERSUB, 0xffffff);
	SSM(sci,SCI_MARKERSETBACK, SC_MARKNUM_FOLDERSUB, 0x000000);
	SSM(sci,SCI_MARKERSETFORE, SC_MARKNUM_FOLDERTAIL, 0xffffff);
	SSM(sci,SCI_MARKERSETBACK, SC_MARKNUM_FOLDERTAIL, 0x000000);
	SSM(sci,SCI_MARKERSETFORE, SC_MARKNUM_FOLDEREND, 0xffffff);
	SSM(sci,SCI_MARKERSETBACK, SC_MARKNUM_FOLDEREND, 0x000000);
	SSM(sci,SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPENMID, 0xffffff);
	SSM(sci,SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPENMID, 0x000000);
	SSM(sci,SCI_MARKERSETFORE, SC_MARKNUM_FOLDERMIDTAIL, 0xffffff);
	SSM(sci,SCI_MARKERSETBACK, SC_MARKNUM_FOLDERMIDTAIL, 0x000000);

	SSM(sci, SCI_SETPROPERTY, (sptr_t) "fold", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "fold.compact", (sptr_t) "0");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "fold.comment", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "fold.preprocessor", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "fold.at.else", (sptr_t) "1");


	SSM(sci, SCI_SETSELFORE, 1, invert(types[GEANY_FILETYPES_ALL]->styling[1][0]));
	SSM(sci, SCI_SETSELBACK, 1, invert(types[GEANY_FILETYPES_ALL]->styling[1][1]));

	SSM (sci, SCI_SETSTYLEBITS, style_bits, 0);


	SSM(sci, SCI_SETFOLDMARGINCOLOUR, 1, invert(types[GEANY_FILETYPES_ALL]->styling[5][1]));
	//SSM(sci, SCI_SETFOLDMARGINHICOLOUR, 1, invert(types[GEANY_FILETYPES_ALL]->styling[5][1]));
	styleset_set_style(sci, STYLE_LINENUMBER, GEANY_FILETYPES_ALL, 4);
	styleset_set_style(sci, STYLE_BRACELIGHT, GEANY_FILETYPES_ALL, 2);
	styleset_set_style(sci, STYLE_BRACEBAD, GEANY_FILETYPES_ALL, 3);
	styleset_set_style(sci, STYLE_INDENTGUIDE, GEANY_FILETYPES_ALL, 8);

	SSM(sci, SCI_SETWHITESPACEFORE, types[GEANY_FILETYPES_ALL]->styling[9][2],
										invert(types[GEANY_FILETYPES_ALL]->styling[9][0]));
	SSM(sci, SCI_SETWHITESPACEBACK, types[GEANY_FILETYPES_ALL]->styling[9][2],
										invert(types[GEANY_FILETYPES_ALL]->styling[9][1]));
}


static void styleset_c_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.c", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.c", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_C] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "comment", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "commentline", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "commentdoc", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "number", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "word", "0x111199", "0xffffff", "true", types[GEANY_FILETYPES_C]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "word2", "0x7f0000", "0xffffff", "true", types[GEANY_FILETYPES_C]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "string", "0x90ff1e", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "character", "0x90ff1e", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "uuid", "0x404080", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "preprocessor", "0x007f7f", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x,00000", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[12]);
	styleset_get_hex(config, config_home, "styling", "stringeol", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_C]->styling[13]);
	styleset_get_hex(config, config_home, "styling", "verbatim", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[14]);
	styleset_get_hex(config, config_home, "styling", "regex", "0x105090", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[15]);
	styleset_get_hex(config, config_home, "styling", "commentlinedoc", "0xff0000", "0xffffff", "true", types[GEANY_FILETYPES_C]->styling[16]);
	styleset_get_hex(config, config_home, "styling", "commentdockeyword", "0xff0000", "0xffffff", "true", types[GEANY_FILETYPES_C]->styling[17]);
	styleset_get_hex(config, config_home, "styling", "globalclass", "0x1111bb", "0xffffff", "true", types[GEANY_FILETYPES_C]->styling[18]);
	styleset_get_int(config, config_home, "styling", "styling_within_preprocessor", 1, 0, types[GEANY_FILETYPES_C]->styling[19]);

	types[GEANY_FILETYPES_C]->keywords = g_new(gchar*, 3);
	styleset_get_keywords(config, config_home, "keywords", "primary", GEANY_FILETYPES_C, 0, "if const struct char int float double void long for while do case switch return");
	styleset_get_keywords(config, config_home, "keywords", "docComment", GEANY_FILETYPES_C, 1, "TODO FIXME");
	types[GEANY_FILETYPES_C]->keywords[2] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_C, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_C);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);

	// load global tags file for C autocompletion
	if (! app->ignore_global_tags && ! global_c_tags_loaded)
	{
		gchar *file = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "global.tags", NULL);
		// 0 is the langType used in TagManager (see the table in tagmanager/parsers.h)
		tm_workspace_load_global_tags(file, 0);
		global_c_tags_loaded = TRUE;
		g_free(file);
	}
}


void styleset_c(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_C] == NULL) styleset_c_init();

	styleset_common(sci, 5);


	/* Assign global keywords */
	if ((app->tm_workspace) && (app->tm_workspace->global_tags))
	{
		guint j;
		GPtrArray *g_typedefs = tm_tags_extract(app->tm_workspace->global_tags, tm_tag_typedef_t | tm_tag_struct_t | tm_tag_class_t);
		if ((g_typedefs) && (g_typedefs->len > 0))
		{
			GString *s = g_string_sized_new(g_typedefs->len * 10);
			for (j = 0; j < g_typedefs->len; ++j)
			{
				if (!(TM_TAG(g_typedefs->pdata[j])->atts.entry.scope))
				{
					g_string_append(s, TM_TAG(g_typedefs->pdata[j])->name);
					g_string_append_c(s, ' ');
				}
			}
			SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) s->str);
			g_string_free(s, TRUE);
		}
		g_ptr_array_free(g_typedefs, TRUE);
	}

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_C]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM(sci, SCI_SETLEXER, SCLEX_CPP, 0);

	//SSM(sci, SCI_SETCONTROLCHARSYMBOL, 32, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_C]->keywords[0]);
	//SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) secondaryKeyWords);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) types[GEANY_FILETYPES_C]->keywords[1]);
	//SSM(sci, SCI_SETKEYWORDS, 4, (sptr_t) typedefsKeyWords);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_C, 0);
	styleset_set_style(sci, SCE_C_DEFAULT, GEANY_FILETYPES_C, 0);
	styleset_set_style(sci, SCE_C_COMMENT, GEANY_FILETYPES_C, 1);
	styleset_set_style(sci, SCE_C_COMMENTLINE, GEANY_FILETYPES_C, 2);
	styleset_set_style(sci, SCE_C_COMMENTDOC, GEANY_FILETYPES_C, 3);
	styleset_set_style(sci, SCE_C_NUMBER, GEANY_FILETYPES_C, 4);
	styleset_set_style(sci, SCE_C_WORD, GEANY_FILETYPES_C, 5);
	styleset_set_style(sci, SCE_C_WORD2, GEANY_FILETYPES_C, 6);
	styleset_set_style(sci, SCE_C_STRING, GEANY_FILETYPES_C, 7);
	styleset_set_style(sci, SCE_C_CHARACTER, GEANY_FILETYPES_C, 8);
	styleset_set_style(sci, SCE_C_UUID, GEANY_FILETYPES_C, 9);
	styleset_set_style(sci, SCE_C_PREPROCESSOR, GEANY_FILETYPES_C, 10);
	styleset_set_style(sci, SCE_C_OPERATOR, GEANY_FILETYPES_C, 11);
	styleset_set_style(sci, SCE_C_IDENTIFIER, GEANY_FILETYPES_C, 12);
	styleset_set_style(sci, SCE_C_STRINGEOL, GEANY_FILETYPES_C, 13);
	styleset_set_style(sci, SCE_C_VERBATIM, GEANY_FILETYPES_C, 14);
	styleset_set_style(sci, SCE_C_REGEX, GEANY_FILETYPES_C, 15);
	styleset_set_style(sci, SCE_C_COMMENTLINEDOC, GEANY_FILETYPES_C, 16);
	styleset_set_style(sci, SCE_C_COMMENTDOCKEYWORD, GEANY_FILETYPES_C, 17);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTDOCKEYWORDERROR, invert(0x0000ff));
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTDOCKEYWORDERROR, invert(0xFFFFFF));
	SSM(sci, SCI_STYLESETITALIC, SCE_C_COMMENTDOCKEYWORDERROR, TRUE);

	// is used for local structs and typedefs
	styleset_set_style(sci, SCE_C_GLOBALCLASS, GEANY_FILETYPES_C, 18);

	if (types[GEANY_FILETYPES_C]->styling[19][0] == 1)
		SSM(sci, SCI_SETPROPERTY, (sptr_t) "styling.within.preprocessor", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "preprocessor.symbol.$(file.patterns.cpp)", (sptr_t) "#");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "preprocessor.start.$(file.patterns.cpp)", (sptr_t) "if ifdef ifndef");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "preprocessor.middle.$(file.patterns.cpp)", (sptr_t) "else elif");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "preprocessor.end.$(file.patterns.cpp)", (sptr_t) "endif");
}


static void styleset_cpp_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.cpp", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.cpp", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_CPP] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_CPP]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "comment", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_CPP]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "commentline", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_CPP]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "commentdoc", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_CPP]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "number", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_CPP]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "word", "0x00007f", "0xffffff", "true", types[GEANY_FILETYPES_CPP]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "word2", "0x991111", "0xffffff", "true", types[GEANY_FILETYPES_CPP]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "string", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_CPP]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "character", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_CPP]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "uuid", "0x404080", "0xffffff", "false", types[GEANY_FILETYPES_CPP]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "preprocessor", "0x007f7f", "0xffffff", "false", types[GEANY_FILETYPES_CPP]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_CPP]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_CPP]->styling[12]);
	styleset_get_hex(config, config_home, "styling", "stringeol", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_CPP]->styling[13]);
	styleset_get_hex(config, config_home, "styling", "verbatim", "0x101030", "0xffffff", "false", types[GEANY_FILETYPES_CPP]->styling[14]);
	styleset_get_hex(config, config_home, "styling", "regex", "0x105090", "0xffffff", "false", types[GEANY_FILETYPES_CPP]->styling[15]);
	styleset_get_hex(config, config_home, "styling", "commentlinedoc", "0xff0000", "0xffffff", "true", types[GEANY_FILETYPES_CPP]->styling[16]);
	styleset_get_hex(config, config_home, "styling", "commentdockeyword", "0xff0000", "0xffffff", "true", types[GEANY_FILETYPES_CPP]->styling[17]);
	styleset_get_hex(config, config_home, "styling", "globalclass", "0x1111bb", "0xffffff", "true", types[GEANY_FILETYPES_CPP]->styling[18]);
	styleset_get_int(config, config_home, "styling", "styling_within_preprocessor", 1, 0, types[GEANY_FILETYPES_CPP]->styling[19]);

	types[GEANY_FILETYPES_CPP]->keywords = g_new(gchar*, 3);
	styleset_get_keywords(config, config_home, "keywords", "primary", GEANY_FILETYPES_CPP, 0, "and and_eq asm auto bitand bitor bool break case catch char class compl const const_cast continue default delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable namespace new not not_eq operator or or_eq private protected public register reinterpret_cast return short signed sizeof static static_cast struct switch template this throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq");
	styleset_get_keywords(config, config_home, "keywords", "docComment", GEANY_FILETYPES_CPP, 1, "TODO FIXME");
	types[GEANY_FILETYPES_CPP]->keywords[2] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_CPP, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_CPP);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);

	// load global tags file for C autocompletion
	if (! app->ignore_global_tags && ! global_c_tags_loaded)
	{
		gchar *file = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "global.tags", NULL);
		// 1 is the langType used in TagManager (see the table in tagmanager/parsers.h)
		tm_workspace_load_global_tags(file, 1);
		global_c_tags_loaded = TRUE;
		g_free(file);
	}
}


void styleset_cpp(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_CPP] == NULL) styleset_cpp_init();

	styleset_common(sci, 5);


	/* Assign global keywords */
	if ((app->tm_workspace) && (app->tm_workspace->global_tags))
	{
		guint j;
		GPtrArray *g_typedefs = tm_tags_extract(app->tm_workspace->global_tags, tm_tag_typedef_t | tm_tag_struct_t | tm_tag_class_t);
		if ((g_typedefs) && (g_typedefs->len > 0))
		{
			GString *s = g_string_sized_new(g_typedefs->len * 10);
			for (j = 0; j < g_typedefs->len; ++j)
			{
				if (!(TM_TAG(g_typedefs->pdata[j])->atts.entry.scope))
				{
					g_string_append(s, TM_TAG(g_typedefs->pdata[j])->name);
					g_string_append_c(s, ' ');
				}
			}
			SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) s->str);
			g_string_free(s, TRUE);
		}
		g_ptr_array_free(g_typedefs, TRUE);
	}

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_CPP]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM(sci, SCI_SETLEXER, SCLEX_CPP, 0);

	//SSM(sci, SCI_SETCONTROLCHARSYMBOL, 32, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_CPP]->keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) types[GEANY_FILETYPES_CPP]->keywords[1]);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_CPP, 0);
	styleset_set_style(sci, SCE_C_DEFAULT, GEANY_FILETYPES_CPP, 0);
	styleset_set_style(sci, SCE_C_COMMENT, GEANY_FILETYPES_CPP, 1);
	styleset_set_style(sci, SCE_C_COMMENTLINE, GEANY_FILETYPES_CPP, 2);
	styleset_set_style(sci, SCE_C_COMMENTDOC, GEANY_FILETYPES_CPP, 3);
	styleset_set_style(sci, SCE_C_NUMBER, GEANY_FILETYPES_CPP, 4);
	styleset_set_style(sci, SCE_C_WORD, GEANY_FILETYPES_CPP, 5);
	styleset_set_style(sci, SCE_C_WORD2, GEANY_FILETYPES_CPP, 6);
	styleset_set_style(sci, SCE_C_STRING, GEANY_FILETYPES_CPP, 7);
	styleset_set_style(sci, SCE_C_CHARACTER, GEANY_FILETYPES_CPP, 8);
	styleset_set_style(sci, SCE_C_UUID, GEANY_FILETYPES_CPP, 9);
	styleset_set_style(sci, SCE_C_PREPROCESSOR, GEANY_FILETYPES_CPP, 10);
	styleset_set_style(sci, SCE_C_OPERATOR, GEANY_FILETYPES_CPP, 11);
	styleset_set_style(sci, SCE_C_IDENTIFIER, GEANY_FILETYPES_CPP, 12);
	styleset_set_style(sci, SCE_C_STRINGEOL, GEANY_FILETYPES_CPP, 13);
	styleset_set_style(sci, SCE_C_VERBATIM, GEANY_FILETYPES_CPP, 14);
	styleset_set_style(sci, SCE_C_REGEX, GEANY_FILETYPES_CPP, 15);
	styleset_set_style(sci, SCE_C_COMMENTLINEDOC, GEANY_FILETYPES_CPP, 16);
	styleset_set_style(sci, SCE_C_COMMENTDOCKEYWORD, GEANY_FILETYPES_CPP, 17);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTDOCKEYWORDERROR, invert(0x0000ff));
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTDOCKEYWORDERROR, invert(0xffffff));
	SSM(sci, SCI_STYLESETITALIC, SCE_C_COMMENTDOCKEYWORDERROR, TRUE);

	// is used for local structs and typedefs
	styleset_set_style(sci, SCE_C_GLOBALCLASS, GEANY_FILETYPES_CPP, 18);

	if (types[GEANY_FILETYPES_CPP]->styling[19][0] == 1)
		SSM(sci, SCI_SETPROPERTY, (sptr_t) "styling.within.preprocessor", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "preprocessor.symbol.$(file.patterns.cpp)", (sptr_t) "#");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "preprocessor.start.$(file.patterns.cpp)", (sptr_t) "if ifdef ifndef");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "preprocessor.middle.$(file.patterns.cpp)", (sptr_t) "else elif");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "preprocessor.end.$(file.patterns.cpp)", (sptr_t) "endif");
}


static void styleset_pascal_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.pascal", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.pascal", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_PASCAL] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "comment", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "number", "0x007F00", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "word", "0x111199", "0xffffff", "true", types[GEANY_FILETYPES_PASCAL]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "string", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "character", "0x404000", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "preprocessor", "0x007f7f", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "regex", "0x1b6313", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "commentline", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "commentdoc", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[11]);

	types[GEANY_FILETYPES_PASCAL]->keywords = g_new(gchar*, 2);
	styleset_get_keywords(config, config_home, "keywords", "primary", GEANY_FILETYPES_PASCAL, 0, "word integer char string byte real \
									for to do until repeat program if uses then else case var begin end \
									asm unit interface implementation procedure function object try class");
	types[GEANY_FILETYPES_PASCAL]->keywords[1] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_PASCAL, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_PASCAL);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_pascal(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_PASCAL] == NULL) styleset_pascal_init();

	styleset_common(sci, 5);

	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_PASCAL]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM (sci, SCI_SETLEXER, SCLEX_PASCAL, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_PASCAL]->keywords[0]);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_PASCAL, 0);
	styleset_set_style(sci, SCE_C_DEFAULT, GEANY_FILETYPES_PASCAL, 0);
	styleset_set_style(sci, SCE_C_COMMENT, GEANY_FILETYPES_PASCAL, 1);
	styleset_set_style(sci, SCE_C_NUMBER, GEANY_FILETYPES_PASCAL, 2);
	styleset_set_style(sci, SCE_C_WORD, GEANY_FILETYPES_PASCAL, 3);
	styleset_set_style(sci, SCE_C_STRING, GEANY_FILETYPES_PASCAL, 4);
	styleset_set_style(sci, SCE_C_CHARACTER, GEANY_FILETYPES_PASCAL, 5);
	styleset_set_style(sci, SCE_C_PREPROCESSOR, GEANY_FILETYPES_PASCAL, 6);
	styleset_set_style(sci, SCE_C_OPERATOR, GEANY_FILETYPES_PASCAL, 7);
	styleset_set_style(sci, SCE_C_IDENTIFIER, GEANY_FILETYPES_PASCAL, 8);
	styleset_set_style(sci, SCE_C_REGEX, GEANY_FILETYPES_PASCAL, 9);
	styleset_set_style(sci, SCE_C_COMMENTLINE, GEANY_FILETYPES_PASCAL, 10);
	styleset_set_style(sci, SCE_C_COMMENTDOC, GEANY_FILETYPES_PASCAL, 11);

	//SSM(sci, SCI_SETPROPERTY, (sptr_t) "styling.within.preprocessor", (sptr_t) "1");
}


static void styleset_makefile_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.makefile", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.makefile", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_MAKE] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x00002f", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "comment", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "preprocessor", "0x007f7f", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "target", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "ideol", "0x008000", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[6]);

	types[GEANY_FILETYPES_MAKE]->keywords = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_MAKE, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_MAKE);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_makefile(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_MAKE] == NULL) styleset_makefile_init();

	styleset_common(sci, 5);

	SSM (sci, SCI_SETLEXER, SCLEX_MAKEFILE, 0);
	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_MAKE]->wordchars);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_MAKE, 0);
	styleset_set_style(sci, SCE_MAKE_DEFAULT, GEANY_FILETYPES_MAKE, 0);
	styleset_set_style(sci, SCE_MAKE_COMMENT, GEANY_FILETYPES_MAKE, 1);
	styleset_set_style(sci, SCE_MAKE_PREPROCESSOR, GEANY_FILETYPES_MAKE, 2);
	styleset_set_style(sci, SCE_MAKE_IDENTIFIER, GEANY_FILETYPES_MAKE, 3);
	styleset_set_style(sci, SCE_MAKE_OPERATOR, GEANY_FILETYPES_MAKE, 4);
	styleset_set_style(sci, SCE_MAKE_TARGET, GEANY_FILETYPES_MAKE, 5);
	styleset_set_style(sci, SCE_MAKE_IDEOL, GEANY_FILETYPES_MAKE, 6);
}


static void styleset_latex_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.latex", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.latex", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_LATEX] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x00002f", "0xffffff", "false", types[GEANY_FILETYPES_LATEX]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "command", "0xff0000", "0xffffff", "true", types[GEANY_FILETYPES_LATEX]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "tag", "0x007f7f", "0xffffff", "true", types[GEANY_FILETYPES_LATEX]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "math", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_LATEX]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "comment", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_LATEX]->styling[4]);

	types[GEANY_FILETYPES_LATEX]->keywords = g_new(gchar*, 2);
	styleset_get_keywords(config, config_home, "keywords", "primary", GEANY_FILETYPES_LATEX, 0, "section subsection begin item");
	types[GEANY_FILETYPES_LATEX]->keywords[1] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_LATEX, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_LATEX);

	// load global tags file for LaTeX autocompletion
	if (! app->ignore_global_tags && ! global_latex_tags_loaded)
	{
		gchar *file = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "latex.tags", NULL);
		// 8 is the langType used in TagManager (see the table in tagmanager/parsers.h)
		tm_workspace_load_global_tags(file, 8);
		global_latex_tags_loaded = TRUE;
		g_free(file);
	}

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_latex(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_LATEX] == NULL) styleset_latex_init();

	styleset_common(sci, 5);

	SSM (sci, SCI_SETLEXER, SCLEX_LATEX, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_LATEX]->keywords[0]);
	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_LATEX]->wordchars);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_LATEX, 0);
	styleset_set_style(sci, SCE_L_DEFAULT, GEANY_FILETYPES_LATEX, 0);
	styleset_set_style(sci, SCE_L_COMMAND, GEANY_FILETYPES_LATEX, 1);
	styleset_set_style(sci, SCE_L_TAG, GEANY_FILETYPES_LATEX, 2);
	styleset_set_style(sci, SCE_L_MATH, GEANY_FILETYPES_LATEX, 3);
	styleset_set_style(sci, SCE_L_COMMENT, GEANY_FILETYPES_LATEX, 4);
}


static void styleset_php_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.php", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.php", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_PHP] = g_new(style_set, 1);
	types[GEANY_FILETYPES_PHP]->keywords = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_PHP, GEANY_WORDCHARS"$");
	filetypes_get_config(config, config_home, GEANY_FILETYPES_PHP);

	// load global tags file for PHP autocompletion
	if (! app->ignore_global_tags && ! global_php_tags_loaded)
	{
		gchar *file = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "php.tags", NULL);
		// 6 is the langType used in TagManager (see the table in tagmanager/parsers.h)
		tm_workspace_load_global_tags(file, 6);
		global_php_tags_loaded = TRUE;
		g_free(file);
	}
	// load global tags file for HTML entities autocompletion
	if (! app->ignore_global_tags && ! global_html_tags_loaded)
	{
		gchar *file = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "html_entities.tags", NULL);
		html_entities = utils_read_file_in_array(file);
		global_html_tags_loaded = TRUE;
		g_free(file);
	}

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_php(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_PHP] == NULL) styleset_php_init();

	styleset_common(sci, 7);

	SSM (sci, SCI_SETPROPERTY, (sptr_t) "phpscript.mode", (sptr_t) "1");
	SSM (sci, SCI_SETLEXER, SCLEX_HTML, 0);

	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	// use the same colouring for HTML; XML and so on
	styleset_markup(sci);

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_PHP]->wordchars);
}


static void styleset_markup_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.xml", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.xml", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_XML] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "html_default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "html_tag", "0x000099", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "html_tagunknown", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "html_attribute", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "html_attributeunknown", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "html_number", "0x800080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "html_doublestring", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "html_singlestring", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "html_other", "0x800080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "html_comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "html_entity", "0x800080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "html_tagend", "0x000080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "html_xmlstart", "0x000099", "0xf0f0f0", "false", types[GEANY_FILETYPES_XML]->styling[12]);
	styleset_get_hex(config, config_home, "styling", "html_xmlend", "0x000099", "0xf0f0f0", "false", types[GEANY_FILETYPES_XML]->styling[13]);
	styleset_get_hex(config, config_home, "styling", "html_script", "0x000080", "0xf0f0f0", "false", types[GEANY_FILETYPES_XML]->styling[14]);
	styleset_get_hex(config, config_home, "styling", "html_asp", "0x004f4f", "0xf0f0f0", "false", types[GEANY_FILETYPES_XML]->styling[15]);
	styleset_get_hex(config, config_home, "styling", "html_aspat", "0x004f4f", "0xf0f0f0", "false", types[GEANY_FILETYPES_XML]->styling[16]);
	styleset_get_hex(config, config_home, "styling", "html_cdata", "0x660099", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[17]);
	styleset_get_hex(config, config_home, "styling", "html_question", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[18]);
	styleset_get_hex(config, config_home, "styling", "html_value", "0x660099", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[19]);
	styleset_get_hex(config, config_home, "styling", "html_xccomment", "0x660099", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[20]);

	styleset_get_hex(config, config_home, "styling", "sgml_default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[21]);
	styleset_get_hex(config, config_home, "styling", "sgml_comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[22]);
	styleset_get_hex(config, config_home, "styling", "sgml_special", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[23]);
	styleset_get_hex(config, config_home, "styling", "sgml_command", "0x111199", "0xffffff", "true", types[GEANY_FILETYPES_XML]->styling[24]);
	styleset_get_hex(config, config_home, "styling", "sgml_doublestring", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[25]);
	styleset_get_hex(config, config_home, "styling", "sgml_simplestring", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[26]);
	styleset_get_hex(config, config_home, "styling", "sgml_1st_param", "0x404080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[27]);
	styleset_get_hex(config, config_home, "styling", "sgml_entity", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[28]);
	styleset_get_hex(config, config_home, "styling", "sgml_block_default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[29]);
	styleset_get_hex(config, config_home, "styling", "sgml_1st_param_comment", "0x406090", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[30]);
	styleset_get_hex(config, config_home, "styling", "sgml_error", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[31]);

	styleset_get_hex(config, config_home, "styling", "php_default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[32]);
	styleset_get_hex(config, config_home, "styling", "php_simplestring", "0x008000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[33]);
	styleset_get_hex(config, config_home, "styling", "php_hstring", "0x008000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[34]);
	styleset_get_hex(config, config_home, "styling", "php_number", "0x606000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[35]);
	styleset_get_hex(config, config_home, "styling", "php_word", "0x000099", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[36]);
	styleset_get_hex(config, config_home, "styling", "php_variable", "0x7f0000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[37]);
	styleset_get_hex(config, config_home, "styling", "php_comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[38]);
	styleset_get_hex(config, config_home, "styling", "php_commentline", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[39]);
	styleset_get_hex(config, config_home, "styling", "php_operator", "0x102060", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[40]);
	styleset_get_hex(config, config_home, "styling", "php_hstring_variable", "0x101060", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[41]);
	styleset_get_hex(config, config_home, "styling", "php_complex_variable", "0x105010", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[42]);

	styleset_get_hex(config, config_home, "styling", "jscript_start", "0x008080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[43]);
	styleset_get_hex(config, config_home, "styling", "jscript_default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[44]);
	styleset_get_hex(config, config_home, "styling", "jscript_comment", "0x222222", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[45]);
	styleset_get_hex(config, config_home, "styling", "jscript_commentline", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[46]);
	styleset_get_hex(config, config_home, "styling", "jscript_commentdoc", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[47]);
	styleset_get_hex(config, config_home, "styling", "jscript_number", "0x006060", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[48]);
	styleset_get_hex(config, config_home, "styling", "jscript_word", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[49]);
	styleset_get_hex(config, config_home, "styling", "jscript_keyword", "0x501010", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[50]);
	styleset_get_hex(config, config_home, "styling", "jscript_doublestring", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[51]);
	styleset_get_hex(config, config_home, "styling", "jscript_singlestring", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[52]);
	styleset_get_hex(config, config_home, "styling", "jscript_symbols", "0x001050", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[53]);
	styleset_get_hex(config, config_home, "styling", "jscript_stringeol", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_XML]->styling[54]);

	types[GEANY_FILETYPES_XML]->keywords = g_new(gchar*, 7);
	styleset_get_keywords(config, config_home, "keywords", "html", GEANY_FILETYPES_XML, 0, "a abbr acronym address applet area b base basefont bdo big blockquote body br button caption center cite code col colgroup dd del dfn dir div dl dt em embed fieldset font form frame frameset h1 h2 h3 h4 h5 h6 head hr html i iframe img input ins isindex kbd label legend li link map menu meta noframes noscript object ol optgroup option p param pre q quality s samp script select small span strike strong style sub sup table tbody td textarea tfoot th thead title tr tt u ul var xmlns leftmargin topmargin abbr accept-charset accept accesskey action align alink alt archive axis background bgcolor border cellpadding cellspacing char charoff charset checked cite class classid clear codebase codetype color cols colspan compact content coords data datafld dataformatas datapagesize datasrc datetime declare defer dir disabled enctype face for frame frameborder selected headers height href hreflang hspace http-equiv id ismap label lang language link longdesc marginwidth marginheight maxlength media framespacing method multiple name nohref noresize noshade nowrap object onblur onchange onclick ondblclick onfocus onkeydown onkeypress onkeyup onload onmousedown onmousemove onmouseover onmouseout onmouseup onreset onselect onsubmit onunload profile prompt pluginspage readonly rel rev rows rowspan rules scheme scope scrolling shape size span src standby start style summary tabindex target text title type usemap valign value valuetype version vlink vspace width text password checkbox radio submit reset file hidden image public doctype xml");
	styleset_get_keywords(config, config_home, "keywords", "javascript", GEANY_FILETYPES_XML, 1, "break this for while null else var false void new delete typeof if in continue true function with return case super extends do const try debugger catch switch finally enum export default class throw import length concat join pop push reverse shift slice splice sort unshift Date Infinity NaN undefined escape eval isFinite isNaN Number parseFloat parseInt string unescape Math abs acos asin atan atan2 ceil cos exp floor log max min pow random round sin sqrt tan MAX_VALUE MIN_VALUE NEGATIVE_INFINITY POSITIVE_INFINITY toString valueOf String length anchor big bold charAt charCodeAt concat fixed fontcolor fontsize fromCharCode indexOf italics lastIndexOf link slice small split strike sub substr substring sup toLowerCase toUpperCase");
	styleset_get_keywords(config, config_home, "keywords", "vbscript", GEANY_FILETYPES_XML, 2, "and as byref byval case call const continue dim do each else elseif end error exit false for function global goto if in loop me new next not nothing on optional or private public redim rem resume select set sub then to true type while with boolean byte currency date double integer long object single string type variant");
	styleset_get_keywords(config, config_home, "keywords", "python", GEANY_FILETYPES_XML, 3, "and assert break class continue complex def del elif else except exec finally for from global if import in inherit is int lambda not or pass print raise return tuple try unicode while yield long float str list");
	styleset_get_keywords(config, config_home, "keywords", "php", GEANY_FILETYPES_XML, 4, "and or xor FILE exception LINE array as break case class const continue declare default die do echo else elseif empty  enddeclare endfor endforeach endif endswitch endwhile eval exit extends for foreach function global if include include_once  isset list new print require require_once return static switch unset use var while FUNCTION CLASS METHOD final php_user_filter interface implements extends public private protected abstract clone try catch throw cfunction old_function this");
	styleset_get_keywords(config, config_home, "keywords", "sgml", GEANY_FILETYPES_XML, 5, "ELEMENT DOCTYPE ATTLIST ENTITY NOTATION");
	types[GEANY_FILETYPES_XML]->keywords[6] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_XML, GEANY_WORDCHARS"$");
	filetypes_get_config(config, config_home, GEANY_FILETYPES_XML);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_markup(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_XML] == NULL) styleset_markup_init();


	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_XML]->keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) types[GEANY_FILETYPES_XML]->keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) types[GEANY_FILETYPES_XML]->keywords[2]);
	SSM(sci, SCI_SETKEYWORDS, 3, (sptr_t) types[GEANY_FILETYPES_XML]->keywords[3]);
	SSM(sci, SCI_SETKEYWORDS, 4, (sptr_t) types[GEANY_FILETYPES_XML]->keywords[4]);
	SSM(sci, SCI_SETKEYWORDS, 5, (sptr_t) types[GEANY_FILETYPES_XML]->keywords[5]);

	// hotspotting, nice thing
	SSM(sci, SCI_SETHOTSPOTACTIVEFORE, 1, invert(0xff0000));
	SSM(sci, SCI_SETHOTSPOTACTIVEUNDERLINE, 1, 0);
	SSM(sci, SCI_SETHOTSPOTSINGLELINE, 1, 0);
	SSM(sci, SCI_STYLESETHOTSPOT, SCE_H_QUESTION, 1);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_XML, 0);
	styleset_set_style(sci, SCE_H_DEFAULT, GEANY_FILETYPES_XML, 0);
	styleset_set_style(sci, SCE_H_TAG, GEANY_FILETYPES_XML, 1);
	styleset_set_style(sci, SCE_H_TAGUNKNOWN, GEANY_FILETYPES_XML, 2);
	styleset_set_style(sci, SCE_H_ATTRIBUTE, GEANY_FILETYPES_XML, 3);
	styleset_set_style(sci, SCE_H_ATTRIBUTEUNKNOWN, GEANY_FILETYPES_XML, 4);
	styleset_set_style(sci, SCE_H_NUMBER, GEANY_FILETYPES_XML, 5);
	styleset_set_style(sci, SCE_H_DOUBLESTRING, GEANY_FILETYPES_XML, 6);
	styleset_set_style(sci, SCE_H_SINGLESTRING, GEANY_FILETYPES_XML, 7);
	styleset_set_style(sci, SCE_H_OTHER, GEANY_FILETYPES_XML, 8);
	styleset_set_style(sci, SCE_H_COMMENT, GEANY_FILETYPES_XML, 9);
	styleset_set_style(sci, SCE_H_ENTITY, GEANY_FILETYPES_XML, 10);
	styleset_set_style(sci, SCE_H_TAGEND, GEANY_FILETYPES_XML, 11);
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_H_XMLSTART, 1);
	styleset_set_style(sci, SCE_H_XMLSTART, GEANY_FILETYPES_XML, 12);
	styleset_set_style(sci, SCE_H_XMLEND, GEANY_FILETYPES_XML, 13);
	styleset_set_style(sci, SCE_H_SCRIPT, GEANY_FILETYPES_XML, 14);
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_H_ASP, 1);
	styleset_set_style(sci, SCE_H_ASP, GEANY_FILETYPES_XML, 15);
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_H_ASPAT, 1);
	styleset_set_style(sci, SCE_H_ASPAT, GEANY_FILETYPES_XML, 16);
	styleset_set_style(sci, SCE_H_CDATA, GEANY_FILETYPES_XML, 17);
	styleset_set_style(sci, SCE_H_QUESTION, GEANY_FILETYPES_XML, 18);
	styleset_set_style(sci, SCE_H_VALUE, GEANY_FILETYPES_XML, 19);
	styleset_set_style(sci, SCE_H_XCCOMMENT, GEANY_FILETYPES_XML, 20);

	styleset_set_style(sci, SCE_H_SGML_DEFAULT, GEANY_FILETYPES_XML, 21);
	styleset_set_style(sci, SCE_H_SGML_COMMENT, GEANY_FILETYPES_XML, 22);
	styleset_set_style(sci, SCE_H_SGML_SPECIAL, GEANY_FILETYPES_XML, 23);
	styleset_set_style(sci, SCE_H_SGML_COMMAND, GEANY_FILETYPES_XML, 24);
	styleset_set_style(sci, SCE_H_SGML_DOUBLESTRING, GEANY_FILETYPES_XML, 25);
	styleset_set_style(sci, SCE_H_SGML_SIMPLESTRING, GEANY_FILETYPES_XML, 26);
	styleset_set_style(sci, SCE_H_SGML_1ST_PARAM, GEANY_FILETYPES_XML, 27);
	styleset_set_style(sci, SCE_H_SGML_ENTITY, GEANY_FILETYPES_XML, 28);
	styleset_set_style(sci, SCE_H_SGML_BLOCK_DEFAULT, GEANY_FILETYPES_XML, 29);
	styleset_set_style(sci, SCE_H_SGML_1ST_PARAM_COMMENT, GEANY_FILETYPES_XML, 30);
	styleset_set_style(sci, SCE_H_SGML_ERROR, GEANY_FILETYPES_XML, 31);

	SSM(sci, SCI_STYLESETFORE, SCE_HB_DEFAULT, invert(0x000000));
	SSM(sci, SCI_STYLESETBACK, SCE_HB_DEFAULT, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HB_COMMENTLINE, invert(0x808080));
	SSM(sci, SCI_STYLESETBACK, SCE_HB_COMMENTLINE, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HB_NUMBER, invert(0x008080));
	SSM(sci, SCI_STYLESETBACK, SCE_HB_NUMBER, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HB_WORD, invert(0x008080));
	SSM(sci, SCI_STYLESETBACK, SCE_HB_WORD, invert(0xffffff));
	SSM(sci, SCI_STYLESETBOLD, SCE_HB_WORD, 1);

	SSM(sci, SCI_STYLESETFORE, SCE_HB_STRING, invert(0x008000));
	SSM(sci, SCI_STYLESETBACK, SCE_HB_STRING, invert(0x008000));

	SSM(sci, SCI_STYLESETFORE, SCE_HB_IDENTIFIER, invert(0x103000));
	SSM(sci, SCI_STYLESETBACK, SCE_HB_IDENTIFIER, invert(0xffffff));
//~ #define SCE_HB_START 70

	// Show the whole section of VBScript
/*	for (bstyle=SCE_HB_DEFAULT; bstyle<=SCE_HB_STRINGEOL; bstyle++) {
		SSM(sci, SCI_STYLESETBACK, bstyle, 0xf5f5f5);
		// This call extends the backround colour of the last style on the line to the edge of the window
		SSM(sci, SCI_STYLESETEOLFILLED, bstyle, 1);
	}
*/
	SSM(sci,SCI_STYLESETBACK, SCE_HB_STRINGEOL, invert(0x7F7FFF));

	SSM(sci, SCI_STYLESETFORE, SCE_HBA_DEFAULT, invert(0x000000));
	SSM(sci, SCI_STYLESETBACK, SCE_HBA_DEFAULT, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HBA_COMMENTLINE, invert(0x808080));
	SSM(sci, SCI_STYLESETBACK, SCE_HBA_COMMENTLINE, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HBA_NUMBER, invert(0x008080));
	SSM(sci, SCI_STYLESETBACK, SCE_HBA_NUMBER, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HBA_WORD, invert(0x008080));
	SSM(sci, SCI_STYLESETBACK, SCE_HBA_WORD, invert(0xffffff));
	SSM(sci, SCI_STYLESETBOLD, SCE_HBA_WORD, 1);

	SSM(sci, SCI_STYLESETFORE, SCE_HBA_STRING, invert(0x008000));
	SSM(sci, SCI_STYLESETBACK, SCE_HBA_STRING, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HBA_IDENTIFIER, invert(0x103000));
	SSM(sci, SCI_STYLESETBACK, SCE_HBA_IDENTIFIER, invert(0xffffff));

	styleset_set_style(sci, SCE_HJ_START, GEANY_FILETYPES_XML, 43);
	styleset_set_style(sci, SCE_HJ_DEFAULT, GEANY_FILETYPES_XML, 44);
	styleset_set_style(sci, SCE_HJ_COMMENT, GEANY_FILETYPES_XML, 45);
	styleset_set_style(sci, SCE_HJ_COMMENTLINE, GEANY_FILETYPES_XML, 46);
	styleset_set_style(sci, SCE_HJ_COMMENTDOC, GEANY_FILETYPES_XML, 47);
	styleset_set_style(sci, SCE_HJ_NUMBER, GEANY_FILETYPES_XML, 48);
	styleset_set_style(sci, SCE_HJ_WORD, GEANY_FILETYPES_XML, 49);
	styleset_set_style(sci, SCE_HJ_KEYWORD, GEANY_FILETYPES_XML, 50);
	styleset_set_style(sci, SCE_HJ_DOUBLESTRING, GEANY_FILETYPES_XML, 51);
	styleset_set_style(sci, SCE_HJ_SINGLESTRING, GEANY_FILETYPES_XML, 52);
	styleset_set_style(sci, SCE_HJ_SYMBOLS, GEANY_FILETYPES_XML, 53);
	styleset_set_style(sci, SCE_HJ_STRINGEOL, GEANY_FILETYPES_XML, 54);


	SSM(sci, SCI_STYLESETFORE, SCE_HP_START, invert(0x808000));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_START, invert(0xf0f0f0));
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_HP_START, 1);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_DEFAULT, invert(0x000000));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_DEFAULT, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HP_COMMENTLINE, invert(0x808080));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_COMMENTLINE, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HP_NUMBER, invert(0x008080));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_NUMBER, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HP_WORD, invert(0x990000));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_WORD, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HP_STRING, invert(0x008000));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_STRING, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HP_CHARACTER, invert(0x006060));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_CHARACTER, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HP_TRIPLE, invert(0x002060));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_TRIPLE, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HP_TRIPLEDOUBLE, invert(0x002060));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_TRIPLEDOUBLE, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HP_CLASSNAME, invert(0x202010));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_CLASSNAME, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HP_CLASSNAME, invert(0x102020));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_CLASSNAME, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HP_OPERATOR, invert(0x602020));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_OPERATOR, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HP_IDENTIFIER, invert(0x001060));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_IDENTIFIER, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_START, invert(0x808000));
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_START, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_DEFAULT, invert(0x000000));
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_DEFAULT, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_COMMENTLINE, invert(0x808080));
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_COMMENTLINE, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_NUMBER, invert(0x408000));
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_NUMBER, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_STRING, invert(0x008080));
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_STRING, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_CHARACTER, invert(0x505080));
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_CHARACTER, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_WORD, invert(0x990000));
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_WORD, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_TRIPLE, invert(0x002060));
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_TRIPLE, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_TRIPLEDOUBLE, invert(0x002060));
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_TRIPLEDOUBLE, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_CLASSNAME, invert(0x202010));
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_CLASSNAME, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_DEFNAME, invert(0x102020));
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_DEFNAME, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_OPERATOR, invert(0x601010));
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_OPERATOR, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_IDENTIFIER, invert(0x105010));
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_IDENTIFIER, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HP_COMMENTLINE, invert(0x808080));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_COMMENTLINE, invert(0xffffff));

	SSM(sci, SCI_STYLESETFORE, SCE_HP_NUMBER, invert(0x408000));
	SSM(sci, SCI_STYLESETBACK, SCE_HP_NUMBER, invert(0xffffff));

	styleset_set_style(sci, SCE_HPHP_DEFAULT, GEANY_FILETYPES_XML, 32);
	styleset_set_style(sci, SCE_HPHP_SIMPLESTRING, GEANY_FILETYPES_XML, 33);
	styleset_set_style(sci, SCE_HPHP_HSTRING, GEANY_FILETYPES_XML, 34);
	styleset_set_style(sci, SCE_HPHP_NUMBER, GEANY_FILETYPES_XML, 35);
	styleset_set_style(sci, SCE_HPHP_WORD, GEANY_FILETYPES_XML, 36);
	styleset_set_style(sci, SCE_HPHP_VARIABLE, GEANY_FILETYPES_XML, 37);
	styleset_set_style(sci, SCE_HPHP_COMMENT, GEANY_FILETYPES_XML, 38);
	styleset_set_style(sci, SCE_HPHP_COMMENTLINE, GEANY_FILETYPES_XML, 39);
	styleset_set_style(sci, SCE_HPHP_OPERATOR, GEANY_FILETYPES_XML, 40);
	styleset_set_style(sci, SCE_HPHP_HSTRING_VARIABLE, GEANY_FILETYPES_XML, 41);
	styleset_set_style(sci, SCE_HPHP_COMPLEX_VARIABLE, GEANY_FILETYPES_XML, 42);

	SSM(sci, SCI_SETPROPERTY, (sptr_t) "fold.html", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "fold.html.preprocessor", (sptr_t) "1");
}


static void styleset_java_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.java", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.java", NULL);

	styleset_load_file(config,  f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_JAVA] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "comment", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "commentline", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "commentdoc", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "number", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "word", "0x111199", "0xffffff", "true", types[GEANY_FILETYPES_JAVA]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "word2", "0x7f0000", "0xffffff", "true", types[GEANY_FILETYPES_JAVA]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "string", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "character", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "uuid", "0x404080", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "preprocessor", "0x007f7f", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x404000", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[12]);
	styleset_get_hex(config, config_home, "styling", "stringeol", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_JAVA]->styling[13]);
	styleset_get_hex(config, config_home, "styling", "verbatim", "0x406090", "0x0000ff", "false", types[GEANY_FILETYPES_JAVA]->styling[14]);
	styleset_get_hex(config, config_home, "styling", "regex", "0x905010", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[15]);
	styleset_get_hex(config, config_home, "styling", "commentlinedoc", "0xff0000", "0xffffff", "true", types[GEANY_FILETYPES_JAVA]->styling[16]);
	styleset_get_hex(config, config_home, "styling", "commentdockeyword", "0xff0000", "0xffffff", "true", types[GEANY_FILETYPES_JAVA]->styling[17]);
	styleset_get_hex(config, config_home, "styling", "globalclass", "0x409010", "0xffffff", "true", types[GEANY_FILETYPES_JAVA]->styling[18]);

	types[GEANY_FILETYPES_JAVA]->keywords = g_new(gchar*, 5);
	styleset_get_keywords(config, config_home, "keywords", "primary", GEANY_FILETYPES_JAVA, 0, "\
										abstract assert break case catch class \
										const continue default do else extends final finally for future \
										generic goto if implements import inner instanceof interface \
										native new outer package private protected public rest \
										return static super switch synchronized this throw throws \
										transient try var volatile while");
	styleset_get_keywords(config, config_home, "keywords", "secondary", GEANY_FILETYPES_JAVA, 1, "boolean byte char double float int long null short void NULL");
	styleset_get_keywords(config, config_home, "keywords", "doccomment", GEANY_FILETYPES_JAVA, 2, "return param author");
	styleset_get_keywords(config, config_home, "keywords", "typedefs", GEANY_FILETYPES_JAVA, 3, "");
	types[GEANY_FILETYPES_JAVA]->keywords[4] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_JAVA, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_JAVA);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_java(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_JAVA] == NULL) styleset_java_init();

	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_CPP, 0);

	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_JAVA]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM (sci, SCI_SETCONTROLCHARSYMBOL, 32, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_JAVA]->keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) types[GEANY_FILETYPES_JAVA]->keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) types[GEANY_FILETYPES_JAVA]->keywords[2]);
	SSM(sci, SCI_SETKEYWORDS, 4, (sptr_t) types[GEANY_FILETYPES_JAVA]->keywords[3]);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_JAVA, 0);
	styleset_set_style(sci, SCE_C_DEFAULT, GEANY_FILETYPES_JAVA, 0);
	styleset_set_style(sci, SCE_C_COMMENT, GEANY_FILETYPES_JAVA, 1);
	styleset_set_style(sci, SCE_C_COMMENTLINE, GEANY_FILETYPES_JAVA, 2);
	styleset_set_style(sci, SCE_C_COMMENTDOC, GEANY_FILETYPES_JAVA, 3);
	styleset_set_style(sci, SCE_C_DEFAULT, GEANY_FILETYPES_JAVA, 4);
	styleset_set_style(sci, SCE_C_WORD, GEANY_FILETYPES_JAVA, 5);
	styleset_set_style(sci, SCE_C_WORD2, GEANY_FILETYPES_JAVA, 6);
	styleset_set_style(sci, SCE_C_STRING, GEANY_FILETYPES_JAVA, 7);
	styleset_set_style(sci, SCE_C_CHARACTER, GEANY_FILETYPES_JAVA, 8);
	styleset_set_style(sci, SCE_C_UUID, GEANY_FILETYPES_JAVA, 9);
	styleset_set_style(sci, SCE_C_PREPROCESSOR, GEANY_FILETYPES_JAVA, 10);
	styleset_set_style(sci, SCE_C_OPERATOR, GEANY_FILETYPES_JAVA, 11);
	styleset_set_style(sci, SCE_C_IDENTIFIER, GEANY_FILETYPES_JAVA, 12);
	styleset_set_style(sci, SCE_C_STRINGEOL, GEANY_FILETYPES_JAVA, 13);
	styleset_set_style(sci, SCE_C_VERBATIM, GEANY_FILETYPES_JAVA, 14);
	styleset_set_style(sci, SCE_C_REGEX, GEANY_FILETYPES_JAVA, 15);
	styleset_set_style(sci, SCE_C_COMMENTLINEDOC, GEANY_FILETYPES_JAVA, 16);
	styleset_set_style(sci, SCE_C_COMMENTDOCKEYWORD, GEANY_FILETYPES_JAVA, 17);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTDOCKEYWORDERROR, invert(0x0000ff));
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTDOCKEYWORDERROR, invert(0xffffff));
	SSM(sci, SCI_STYLESETITALIC, SCE_C_COMMENTDOCKEYWORDERROR, TRUE);

	styleset_set_style(sci, SCE_C_GLOBALCLASS, GEANY_FILETYPES_JAVA, 18);
}


static void styleset_perl_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.perl", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.perl", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_PERL] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "error", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "commentline", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "number", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "word", "0x111199", "0xffffff", "true", types[GEANY_FILETYPES_PERL]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "string", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "character", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "preprocessor", "0x007f7f", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "scalar", "0x7f0000", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "pod", "0x035650", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "regex", "0x105090", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[12]);
	styleset_get_hex(config, config_home, "styling", "array", "0x105090", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[13]);
	styleset_get_hex(config, config_home, "styling", "hash", "0x105090", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[14]);
	styleset_get_hex(config, config_home, "styling", "symboltable", "0x105090", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[15]);
	styleset_get_hex(config, config_home, "styling", "backticks", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_PERL]->styling[16]);

	types[GEANY_FILETYPES_PERL]->keywords = g_new(gchar*, 2);
	styleset_get_keywords(config, config_home, "keywords", "primary", GEANY_FILETYPES_PERL, 0, "\
									NULL __FILE__ __LINE__ __PACKAGE__ __DATA__ __END__ AUTOLOAD \
									BEGIN CORE DESTROY END EQ GE GT INIT LE LT NE CHECK abs accept \
									alarm and atan2 bind binmode bless caller chdir chmod chomp chop \
									chown chr chroot close closedir cmp connect continue cos crypt \
									dbmclose dbmopen defined delete die do dump each else elsif endgrent \
									endhostent endnetent endprotoent endpwent endservent eof eq eval \
									exec exists exit exp fcntl fileno flock for foreach fork format \
									formline ge getc getgrent getgrgid getgrnam gethostbyaddr gethostbyname \
									gethostent getlogin getnetbyaddr getnetbyname getnetent getpeername \
									getpgrp getppid getpriority getprotobyname getprotobynumber getprotoent \
									getpwent getpwnam getpwuid getservbyname getservbyport getservent \
									getsockname getsockopt glob gmtime goto grep gt hex if index \
									int ioctl join keys kill last lc lcfirst le length link listen \
									local localtime lock log lstat lt m map mkdir msgctl msgget msgrcv \
									msgsnd my ne next no not oct open opendir or ord our pack package \
									pipe pop pos print printf prototype push q qq qr quotemeta qu \
									qw qx rand read readdir readline readlink readpipe recv redo \
									ref rename require reset return reverse rewinddir rindex rmdir \
									s scalar seek seekdir select semctl semget semop send setgrent \
									sethostent setnetent setpgrp setpriority setprotoent setpwent \
									setservent setsockopt shift shmctl shmget shmread shmwrite shutdown \
									sin sleep socket socketpair sort splice split sprintf sqrt srand \
									stat study sub substr symlink syscall sysopen sysread sysseek \
									system syswrite tell telldir tie tied time times tr truncate \
									uc ucfirst umask undef unless unlink unpack unshift untie until \
									use utime values vec wait waitpid wantarray warn while write \
									x xor y");
	types[GEANY_FILETYPES_PERL]->keywords[1] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_PERL, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_PERL);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_perl(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_PERL] == NULL) styleset_perl_init();

	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_PERL, 0);

	SSM(sci, SCI_SETPROPERTY, (sptr_t) "styling.within.preprocessor", (sptr_t) "1");

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_PERL]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_PERL]->keywords[0]);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_PERL, 0);
	styleset_set_style(sci, SCE_PL_DEFAULT, GEANY_FILETYPES_PERL, 0);
	styleset_set_style(sci, SCE_PL_ERROR, GEANY_FILETYPES_PERL, 1);
	styleset_set_style(sci, SCE_PL_COMMENTLINE, GEANY_FILETYPES_PERL, 2);
	styleset_set_style(sci, SCE_PL_NUMBER, GEANY_FILETYPES_PERL, 3);
	styleset_set_style(sci, SCE_PL_WORD, GEANY_FILETYPES_PERL, 4);
	styleset_set_style(sci, SCE_PL_STRING, GEANY_FILETYPES_PERL, 5);
	styleset_set_style(sci, SCE_PL_CHARACTER, GEANY_FILETYPES_PERL, 6);
	styleset_set_style(sci, SCE_PL_PREPROCESSOR, GEANY_FILETYPES_PERL, 7);
	styleset_set_style(sci, SCE_PL_OPERATOR, GEANY_FILETYPES_PERL, 8);
	styleset_set_style(sci, SCE_PL_IDENTIFIER, GEANY_FILETYPES_PERL, 9);
	styleset_set_style(sci, SCE_PL_SCALAR, GEANY_FILETYPES_PERL, 10);
	styleset_set_style(sci, SCE_PL_POD, GEANY_FILETYPES_PERL, 11);
	styleset_set_style(sci, SCE_PL_REGEX, GEANY_FILETYPES_PERL, 12);
	styleset_set_style(sci, SCE_PL_ARRAY, GEANY_FILETYPES_PERL, 13);
	styleset_set_style(sci, SCE_PL_BACKTICKS, GEANY_FILETYPES_PERL, 14);
	styleset_set_style(sci, SCE_PL_HASH, GEANY_FILETYPES_PERL, 15);
	styleset_set_style(sci, SCE_PL_SYMBOLTABLE, GEANY_FILETYPES_PERL, 16);
}


static void styleset_python_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.python", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.python", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_PYTHON] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "commentline", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "number", "0x400080", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "string", "0x008000", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "character", "0x008000", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "word", "0x600080", "0xffffff", "true", types[GEANY_FILETYPES_PYTHON]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "triple", "0x008020", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "tripledouble", "0x404000", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "classname", "0x003030", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "defname", "0x000080", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x300080", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "commentblock", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[12]);
	styleset_get_hex(config, config_home, "styling", "stringeol", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_PYTHON]->styling[13]);

	types[GEANY_FILETYPES_PYTHON]->keywords = g_new(gchar*, 2);
	styleset_get_keywords(config, config_home, "keywords", "primary", GEANY_FILETYPES_PYTHON, 0, "and assert break class continue def del elif else except exec finally for from global if import in is lambda not or pass print raise return try while yield");
	types[GEANY_FILETYPES_PYTHON]->keywords[1] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_PYTHON, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_PYTHON);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_python(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_PYTHON] == NULL) styleset_python_init();

	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_PYTHON, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_PYTHON]->keywords[0]);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_PYTHON, 0);
	styleset_set_style(sci, SCE_P_DEFAULT, GEANY_FILETYPES_PYTHON, 0);
	styleset_set_style(sci, SCE_P_COMMENTLINE, GEANY_FILETYPES_PYTHON, 1);
	styleset_set_style(sci, SCE_P_NUMBER, GEANY_FILETYPES_PYTHON, 2);
	styleset_set_style(sci, SCE_P_STRING, GEANY_FILETYPES_PYTHON, 3);
	styleset_set_style(sci, SCE_P_CHARACTER, GEANY_FILETYPES_PYTHON, 4);
	styleset_set_style(sci, SCE_P_WORD, GEANY_FILETYPES_PYTHON, 5);
	styleset_set_style(sci, SCE_P_TRIPLE, GEANY_FILETYPES_PYTHON, 6);
	styleset_set_style(sci, SCE_P_TRIPLEDOUBLE, GEANY_FILETYPES_PYTHON, 7);
	styleset_set_style(sci, SCE_P_CLASSNAME, GEANY_FILETYPES_PYTHON, 8);
	styleset_set_style(sci, SCE_P_DEFNAME, GEANY_FILETYPES_PYTHON, 9);
	styleset_set_style(sci, SCE_P_OPERATOR, GEANY_FILETYPES_PYTHON, 10);
	styleset_set_style(sci, SCE_P_IDENTIFIER, GEANY_FILETYPES_PYTHON, 11);
	styleset_set_style(sci, SCE_P_COMMENTBLOCK, GEANY_FILETYPES_PYTHON, 12);
	styleset_set_style(sci, SCE_P_STRINGEOL, GEANY_FILETYPES_PYTHON, 13);

	SSM(sci, SCI_SETPROPERTY, (sptr_t) "fold.comment.python", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "fold.quotes.python", (sptr_t) "1");
}


static void styleset_ruby_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir,  G_DIR_SEPARATOR_S "filetypes.ruby", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.ruby", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_RUBY] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_RUBY]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "commentline", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_RUBY]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "number", "0x400080", "0xffffff", "false", types[GEANY_FILETYPES_RUBY]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "string", "0x008000", "0xffffff", "false", types[GEANY_FILETYPES_RUBY]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "character", "0x008000", "0xffffff", "false", types[GEANY_FILETYPES_RUBY]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "word", "0x111199", "0xffffff", "true", types[GEANY_FILETYPES_RUBY]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "global", "0x111199", "0xffffff", "false", types[GEANY_FILETYPES_RUBY]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "symbol", "0x008020", "0xffffff", "false", types[GEANY_FILETYPES_RUBY]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "classname", "0x7f0000", "0xffffff", "true", types[GEANY_FILETYPES_RUBY]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "defname", "0x7f0000", "0xffffff", "false", types[GEANY_FILETYPES_RUBY]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_RUBY]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_RUBY]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "modulename", "0x111199", "0xffffff", "true", types[GEANY_FILETYPES_RUBY]->styling[12]);
	styleset_get_hex(config, config_home, "styling", "backticks", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_RUBY]->styling[13]);
	styleset_get_hex(config, config_home, "styling", "instancevar", "0x000000", "0xffffff", "true", types[GEANY_FILETYPES_RUBY]->styling[14]);
	styleset_get_hex(config, config_home, "styling", "classvar", "0x000000", "0xffffff", "true", types[GEANY_FILETYPES_RUBY]->styling[15]);
	styleset_get_hex(config, config_home, "styling", "datasection", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_RUBY]->styling[16]);
	styleset_get_hex(config, config_home, "styling", "heredelim", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_RUBY]->styling[17]);
	styleset_get_hex(config, config_home, "styling", "worddemoted", "0x111199", "0xffffff", "false", types[GEANY_FILETYPES_RUBY]->styling[18]);

	types[GEANY_FILETYPES_RUBY]->keywords = g_new(gchar*, 2);
	styleset_get_keywords(config, config_home, "keywords", "primary", GEANY_FILETYPES_RUBY, 0, "load define_method attr_accessor attr_writer attr_reader include __FILE__ and def end in or self unless __LINE__ begin defined? ensure module redo super until BEGIN break do false next rescue then when END case else for nil require retry true while alias class elsif if not return undef yield");
	types[GEANY_FILETYPES_RUBY]->keywords[1] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_RUBY, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_RUBY);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_ruby(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_RUBY] == NULL) styleset_ruby_init();

	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_RUBY, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_RUBY]->keywords[0]);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_RUBY, 0);
	styleset_set_style(sci, SCE_RB_DEFAULT, GEANY_FILETYPES_RUBY, 0);
	styleset_set_style(sci, SCE_RB_COMMENTLINE, GEANY_FILETYPES_RUBY, 1);
	styleset_set_style(sci, SCE_RB_NUMBER, GEANY_FILETYPES_RUBY, 2);
	styleset_set_style(sci, SCE_RB_STRING, GEANY_FILETYPES_RUBY, 3);
	styleset_set_style(sci, SCE_RB_CHARACTER, GEANY_FILETYPES_RUBY, 4);
	styleset_set_style(sci, SCE_RB_WORD, GEANY_FILETYPES_RUBY, 5);
	styleset_set_style(sci, SCE_RB_GLOBAL, GEANY_FILETYPES_RUBY, 6);
	styleset_set_style(sci, SCE_RB_SYMBOL, GEANY_FILETYPES_RUBY, 7);
	styleset_set_style(sci, SCE_RB_CLASSNAME, GEANY_FILETYPES_RUBY, 8);
	styleset_set_style(sci, SCE_RB_DEFNAME, GEANY_FILETYPES_RUBY, 9);
	styleset_set_style(sci, SCE_RB_OPERATOR, GEANY_FILETYPES_RUBY, 10);
	styleset_set_style(sci, SCE_RB_IDENTIFIER, GEANY_FILETYPES_RUBY, 11);
	styleset_set_style(sci, SCE_RB_MODULE_NAME, GEANY_FILETYPES_RUBY, 12);
	styleset_set_style(sci, SCE_RB_BACKTICKS, GEANY_FILETYPES_RUBY, 13);
	styleset_set_style(sci, SCE_RB_INSTANCE_VAR, GEANY_FILETYPES_RUBY, 14);
	styleset_set_style(sci, SCE_RB_CLASS_VAR, GEANY_FILETYPES_RUBY, 15);
	styleset_set_style(sci, SCE_RB_DATASECTION, GEANY_FILETYPES_RUBY, 16);
	styleset_set_style(sci, SCE_RB_HERE_DELIM, GEANY_FILETYPES_RUBY, 17);
	styleset_set_style(sci, SCE_RB_WORD_DEMOTED, GEANY_FILETYPES_RUBY, 18);
}


static void styleset_sh_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.sh", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.sh", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_SH] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "commentline", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "number", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "word", "0x119911", "0xffffff", "true", types[GEANY_FILETYPES_SH]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "string", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "character", "0x404000", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "backticks", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_SH]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "param", "0x9f0000", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "scalar", "0x105090", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[10]);

	types[GEANY_FILETYPES_SH]->keywords = g_new(gchar*, 2);
	styleset_get_keywords(config, config_home, "keywords", "primary", GEANY_FILETYPES_SH, 0, "break case continue do done elif else esac eval exit export fi for goto if in integer return set shift then while");
	types[GEANY_FILETYPES_SH]->keywords[1] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_SH, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_SH);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_sh(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_SH] == NULL) styleset_sh_init();

	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_BASH, 0);

	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_SH]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM (sci, SCI_SETCONTROLCHARSYMBOL, 32, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_SH]->keywords[0]);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_SH, 0);
	styleset_set_style(sci, SCE_SH_DEFAULT, GEANY_FILETYPES_SH, 0);
	styleset_set_style(sci, SCE_SH_COMMENTLINE, GEANY_FILETYPES_SH, 1);
	styleset_set_style(sci, SCE_SH_NUMBER, GEANY_FILETYPES_SH, 2);
	styleset_set_style(sci, SCE_SH_WORD, GEANY_FILETYPES_SH, 3);
	styleset_set_style(sci, SCE_SH_STRING, GEANY_FILETYPES_SH, 4);
	styleset_set_style(sci, SCE_SH_CHARACTER, GEANY_FILETYPES_SH, 5);
	styleset_set_style(sci, SCE_SH_OPERATOR, GEANY_FILETYPES_SH, 6);
	styleset_set_style(sci, SCE_SH_IDENTIFIER, GEANY_FILETYPES_SH, 7);
	styleset_set_style(sci, SCE_SH_BACKTICKS, GEANY_FILETYPES_SH, 8);
	styleset_set_style(sci, SCE_SH_PARAM, GEANY_FILETYPES_SH, 9);
	styleset_set_style(sci, SCE_SH_SCALAR, GEANY_FILETYPES_SH, 10);
}


void styleset_xml(ScintillaObject *sci)
{
	styleset_common(sci, 7);

	SSM (sci, SCI_SETLEXER, SCLEX_XML, 0);

	// use the same colouring for HTML; XML and so on
	styleset_markup(sci);

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_XML]->wordchars);
}


static void styleset_docbook_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.docbook", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.docbook", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_DOCBOOK] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "tag", "0x000099", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "tagunknown", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "attribute", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "attributeunknown", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "number", "0x800080", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "doublestring", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "singlestring", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "other", "0x800080", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "entity", "0x800080", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "tagend", "0x000099", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "xmlstart", "0x000099", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[12]);
	styleset_get_hex(config, config_home, "styling", "xmlend", "0x000099", "0xf0f0f0", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[13]);
	styleset_get_hex(config, config_home, "styling", "cdata", "0x660099", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[14]);
	styleset_get_hex(config, config_home, "styling", "question", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[15]);
	styleset_get_hex(config, config_home, "styling", "value", "0x660099", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[16]);
	styleset_get_hex(config, config_home, "styling", "xccomment", "0x660099", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[17]);
	styleset_get_hex(config, config_home, "styling", "sgml_default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[18]);
	styleset_get_hex(config, config_home, "styling", "sgml_comment", "0x303030", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[19]);
	styleset_get_hex(config, config_home, "styling", "sgml_special", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[20]);
	styleset_get_hex(config, config_home, "styling", "sgml_command", "0x111199", "0xffffff", "true", types[GEANY_FILETYPES_DOCBOOK]->styling[21]);
	styleset_get_hex(config, config_home, "styling", "sgml_doublestring", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[22]);
	styleset_get_hex(config, config_home, "styling", "sgml_simplestring", "0x404000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[23]);
	styleset_get_hex(config, config_home, "styling", "sgml_1st_param", "0x404080", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[24]);
	styleset_get_hex(config, config_home, "styling", "sgml_entity", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[25]);
	styleset_get_hex(config, config_home, "styling", "sgml_block_default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[26]);
	styleset_get_hex(config, config_home, "styling", "sgml_1st_param_comment", "0x406090", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[27]);
	styleset_get_hex(config, config_home, "styling", "sgml_error", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[28]);

	types[GEANY_FILETYPES_DOCBOOK]->keywords = g_new(gchar*, 3);
	styleset_get_keywords(config, config_home, "keywords", "elements", GEANY_FILETYPES_DOCBOOK, 0,
		   "abbrev abstract accel ackno acronym action address affiliation alt anchor \
			answer appendix appendixinfo application area areaset areaspec arg article \
			articleinfo artpagenums attribution audiodata audioobject author authorblurb \
			authorgroup authorinitials beginpage bibliocoverage bibliodiv biblioentry \
			bibliography bibliographyinfo biblioid bibliomisc bibliomixed bibliomset \
			bibliorelation biblioset bibliosource blockinfo blockquote book bookinfo \
			bridgehead callout calloutlist caption caution chapter chapterinfo citation \
			citebiblioid citerefentry citetitle city classname classsynopsis classsynopsisinfo \
			cmdsynopsis co collab collabname colophon nameend namest colname colspec command computeroutput \
			confdates confgroup confnum confsponsor conftitle constant constraint \
			constraintdef constructorsynopsis contractnum contractsponsor contrib \
			copyright coref corpauthor corpname country database date dedication \
			destructorsynopsis edition editor email emphasis entry entrytbl envar \
			epigraph equation errorcode errorname errortext errortype example \
			exceptionname fax fieldsynopsis figure filename fileref firstname firstterm \
			footnote footnoteref foreignphrase formalpara frame funcdef funcparams \
			funcprototype funcsynopsis funcsynopsisinfo function glossary glossaryinfo \
			glossdef glossdiv glossentry glosslist glosssee glossseealso glossterm \
			graphic graphicco group guibutton guiicon guilabel guimenu guimenuitem \
			guisubmenu hardware highlights holder honorific htm imagedata imageobject \
			imageobjectco important index indexdiv indexentry indexinfo indexterm \
			informalequation informalexample informalfigure informaltable initializer \
			inlineequation inlinegraphic inlinemediaobject interface interfacename \
			invpartnumber isbn issn issuenum itemizedlist itermset jobtitle keycap \
			keycode keycombo keysym keyword keywordset label legalnotice lhs lineage \
			lineannotation link listitem iteral literallayout lot lotentry manvolnum \
			markup medialabel mediaobject mediaobjectco member menuchoice methodname \
			methodparam methodsynopsis mm modespec modifier ousebutton msg msgaud \
			msgentry msgexplan msginfo msglevel msgmain msgorig msgrel msgset msgsub \
			msgtext nonterminal note objectinfo olink ooclass ooexception oointerface \
			option optional orderedlist orgdiv orgname otheraddr othercredit othername \
			pagenums para paramdef parameter part partinfo partintro personblurb \
			personname phone phrase pob postcode preface prefaceinfo primary primaryie \
			printhistory procedure production productionrecap productionset productname \
			productnumber programlisting programlistingco prompt property pubdate publisher \
			publishername pubsnumber qandadiv qandaentry qandaset question quote refclass \
			refdescriptor refentry refentryinfo refentrytitle reference referenceinfo \
			refmeta refmiscinfo refname refnamediv refpurpose refsect1 refsect1info refsect2 \
			refsect2info refsect3 refsect3info refsection refsectioninfo refsynopsisdiv \
			refsynopsisdivinfo releaseinfo remark replaceable returnvalue revdescription \
			revhistory revision revnumber revremark rhs row sbr screen screenco screeninfo \
			screenshot secondary secondaryie sect1 sect1info sect2 sect2info sect3 sect3info \
			sect4 sect4info sect5 sect5info section sectioninfo see seealso seealsoie \
			seeie seg seglistitem segmentedlist segtitle seriesvolnums set setindex \
			setindexinfo setinfo sgmltag shortaffil shortcut sidebar sidebarinfo simpara \
			simplelist simplemsgentry simplesect spanspec state step street structfield \
			structname subject subjectset subjectterm subscript substeps subtitle \
			superscript surname sv symbol synopfragment synopfragmentref synopsis \
			systemitem table tbody term tertiary tertiaryie textdata textobject tfoot \
			tgroup thead tip title titleabbrev toc tocback tocchap tocentry tocfront \
			toclevel1 toclevel2 toclevel3 toclevel4 toclevel5 tocpart token trademark \
			type ulink userinput varargs variablelist varlistentry varname videodata \
			videoobject void volumenum warning wordasword xref year cols colnum align spanname\
			arch condition conformance id lang os remap role revision revisionflag security \
			userlevel url vendor xreflabel status label endterm linkend space width");
	styleset_get_keywords(config, config_home, "keywords", "dtd", GEANY_FILETYPES_DOCBOOK, 1, "ELEMENT DOCTYPE ATTLIST ENTITY NOTATION");
	types[GEANY_FILETYPES_DOCBOOK]->keywords[2] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_DOCBOOK, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_DOCBOOK);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_docbook(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_DOCBOOK] == NULL) styleset_docbook_init();

	styleset_common(sci, 7);
	SSM (sci, SCI_SETLEXER, SCLEX_XML, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_DOCBOOK]->keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 5, (sptr_t) types[GEANY_FILETYPES_DOCBOOK]->keywords[1]);

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_DOCBOOK]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	// Unknown tags and attributes are highlighed in red.
	// If a tag is actually OK, it should be added in lower case to the htmlKeyWords string.

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_DOCBOOK, 0);
	styleset_set_style(sci, SCE_H_DEFAULT, GEANY_FILETYPES_DOCBOOK, 0);
	styleset_set_style(sci, SCE_H_TAG, GEANY_FILETYPES_DOCBOOK, 1);
	styleset_set_style(sci, SCE_H_TAGUNKNOWN, GEANY_FILETYPES_DOCBOOK, 2);
	styleset_set_style(sci, SCE_H_ATTRIBUTE, GEANY_FILETYPES_DOCBOOK, 3);
	styleset_set_style(sci, SCE_H_ATTRIBUTEUNKNOWN, GEANY_FILETYPES_DOCBOOK, 4);
	styleset_set_style(sci, SCE_H_NUMBER, GEANY_FILETYPES_DOCBOOK, 5);
	styleset_set_style(sci, SCE_H_DOUBLESTRING, GEANY_FILETYPES_DOCBOOK, 6);
	styleset_set_style(sci, SCE_H_SINGLESTRING, GEANY_FILETYPES_DOCBOOK, 7);
	styleset_set_style(sci, SCE_H_OTHER, GEANY_FILETYPES_DOCBOOK, 8);
	styleset_set_style(sci, SCE_H_COMMENT, GEANY_FILETYPES_DOCBOOK, 9);
	styleset_set_style(sci, SCE_H_ENTITY, GEANY_FILETYPES_DOCBOOK, 10);
	styleset_set_style(sci, SCE_H_TAGEND, GEANY_FILETYPES_DOCBOOK, 11);
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_H_XMLSTART, 1);
	styleset_set_style(sci, SCE_H_XMLSTART, GEANY_FILETYPES_DOCBOOK, 12);
	styleset_set_style(sci, SCE_H_XMLEND, GEANY_FILETYPES_DOCBOOK, 13);
	styleset_set_style(sci, SCE_H_CDATA, GEANY_FILETYPES_DOCBOOK, 14);
	styleset_set_style(sci, SCE_H_QUESTION, GEANY_FILETYPES_DOCBOOK, 15);
	styleset_set_style(sci, SCE_H_VALUE, GEANY_FILETYPES_DOCBOOK, 16);
	styleset_set_style(sci, SCE_H_XCCOMMENT, GEANY_FILETYPES_DOCBOOK, 17);
	styleset_set_style(sci, SCE_H_SGML_DEFAULT, GEANY_FILETYPES_DOCBOOK, 18);
	styleset_set_style(sci, SCE_H_DEFAULT, GEANY_FILETYPES_DOCBOOK, 19);
	styleset_set_style(sci, SCE_H_SGML_SPECIAL, GEANY_FILETYPES_DOCBOOK, 20);
	styleset_set_style(sci, SCE_H_SGML_COMMAND, GEANY_FILETYPES_DOCBOOK, 21);
	styleset_set_style(sci, SCE_H_SGML_DOUBLESTRING, GEANY_FILETYPES_DOCBOOK, 22);
	styleset_set_style(sci, SCE_H_SGML_SIMPLESTRING, GEANY_FILETYPES_DOCBOOK, 23);
	styleset_set_style(sci, SCE_H_SGML_1ST_PARAM, GEANY_FILETYPES_DOCBOOK, 24);
	styleset_set_style(sci, SCE_H_SGML_ENTITY, GEANY_FILETYPES_DOCBOOK, 25);
	styleset_set_style(sci, SCE_H_SGML_BLOCK_DEFAULT, GEANY_FILETYPES_DOCBOOK, 26);
	styleset_set_style(sci, SCE_H_SGML_1ST_PARAM_COMMENT, GEANY_FILETYPES_DOCBOOK, 27);
	styleset_set_style(sci, SCE_H_SGML_ERROR, GEANY_FILETYPES_DOCBOOK, 28);

	SSM(sci, SCI_SETPROPERTY, (sptr_t) "fold.html", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "fold.html.preprocessor", (sptr_t) "1");
}


void styleset_none(ScintillaObject *sci)
{
	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_NULL, 0);

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_ALL]->wordchars);
}


static void styleset_css_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.css", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.css", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_CSS] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x003399", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "tag", "0x2166a4", "0xffffff", "true", types[GEANY_FILETYPES_CSS]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "class", "0x007f00", "0xffffff", "true", types[GEANY_FILETYPES_CSS]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "pseudoclass", "0x660010", "0xffffff", "true", types[GEANY_FILETYPES_CSS]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "unknown_pseudoclass", "0x000099", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "unknown_identifier", "0x000099", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x000099", "0xffffff", "true", types[GEANY_FILETYPES_CSS]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "doublestring", "0x330066", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "singlestring", "0x330066", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "attribute", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "value", "0x303030", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[12]);

	types[GEANY_FILETYPES_CSS]->keywords = g_new(gchar*, 4);
	styleset_get_keywords(config, config_home, "keywords", "primary", GEANY_FILETYPES_CSS, 0,
								"color background-color background-image background-repeat background-attachment background-position background \
								font-family font-style font-variant font-weight font-size font \
								word-spacing letter-spacing text-decoration vertical-align text-transform text-align text-indent line-height \
								margin-top margin-right margin-bottom margin-left margin \
								padding-top padding-right padding-bottom padding-left padding \
								border-top-width border-right-width border-bottom-width border-left-width border-width \
								border-top border-right border-bottom border-left border \
								border-color border-style width height float clear \
								display white-space list-style-type list-style-image list-style-position list-style");
	styleset_get_keywords(config, config_home, "keywords", "secondary", GEANY_FILETYPES_CSS, 1,
								"border-top-color border-right-color border-bottom-color border-left-color border-color \
								border-top-style border-right-style border-bottom-style border-left-style border-style \
								top right bottom left position z-index direction unicode-bidi \
								min-width max-width min-height max-height overflow clip visibility content quotes \
								counter-reset counter-increment marker-offset \
								size marks page-break-before page-break-after page-break-inside page orphans widows \
								font-stretch font-size-adjust unicode-range units-per-em src \
								panose-1 stemv stemh slope cap-height x-height ascent descent widths bbox definition-src \
								baseline centerline mathline topline text-shadow \
								caption-side table-layout border-collapse border-spacing empty-cells speak-header \
								cursor outline outline-width outline-style outline-color \
								volume speak pause-before pause-after pause cue-before cue-after cue \
								play-during azimuth elevation speech-rate voice-family pitch pitch-range stress richness \
								speak-punctuation speak-numeral");
	styleset_get_keywords(config, config_home, "keywords", "pseudoclasses", GEANY_FILETYPES_CSS, 2, "first-letter first-line link active visited lang first-child focus hover before after left right first");
	types[GEANY_FILETYPES_CSS]->keywords[3] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_CSS, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_CSS);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_css(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_CSS] == NULL) styleset_css_init();

	styleset_common(sci, 5);

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_CSS]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM(sci, SCI_SETLEXER, SCLEX_CSS, 0);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_CSS, 0);
	styleset_set_style(sci, SCE_CSS_DEFAULT, GEANY_FILETYPES_CSS, 0);
	styleset_set_style(sci, SCE_CSS_COMMENT, GEANY_FILETYPES_CSS, 1);
	styleset_set_style(sci, SCE_CSS_TAG, GEANY_FILETYPES_CSS, 2);
	styleset_set_style(sci, SCE_CSS_CLASS, GEANY_FILETYPES_CSS, 3);
	styleset_set_style(sci, SCE_CSS_PSEUDOCLASS, GEANY_FILETYPES_CSS, 4);
	styleset_set_style(sci, SCE_CSS_UNKNOWN_PSEUDOCLASS, GEANY_FILETYPES_CSS, 5);
	styleset_set_style(sci, SCE_CSS_UNKNOWN_IDENTIFIER, GEANY_FILETYPES_CSS, 6);
	styleset_set_style(sci, SCE_CSS_OPERATOR, GEANY_FILETYPES_CSS, 7);
	styleset_set_style(sci, SCE_CSS_IDENTIFIER, GEANY_FILETYPES_CSS, 8);
	styleset_set_style(sci, SCE_CSS_DOUBLESTRING, GEANY_FILETYPES_CSS, 9);
	styleset_set_style(sci, SCE_CSS_SINGLESTRING, GEANY_FILETYPES_CSS, 10);
	styleset_set_style(sci, SCE_CSS_ATTRIBUTE, GEANY_FILETYPES_CSS, 11);
	styleset_set_style(sci, SCE_CSS_VALUE, GEANY_FILETYPES_CSS, 12);
}


static void styleset_conf_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.conf", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.conf", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_CONF] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x7f0000", "0xffffff", "false", types[GEANY_FILETYPES_CONF]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_CONF]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "section", "0x000090", "0xffffff", "true", types[GEANY_FILETYPES_CONF]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "key", "0x00007f", "0xffffff", "false", types[GEANY_FILETYPES_CONF]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "assignment", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_CONF]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "defval", "0x00007f", "0xffffff", "false", types[GEANY_FILETYPES_CONF]->styling[5]);

	types[GEANY_FILETYPES_CONF]->keywords = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_CONF, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_CONF);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_conf(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_CONF] == NULL) styleset_conf_init();

	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_PROPERTIES, 0);

	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_CONF]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM (sci, SCI_SETCONTROLCHARSYMBOL, 32, 0);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_CONF, 0);
	styleset_set_style(sci, SCE_PROPS_DEFAULT, GEANY_FILETYPES_CONF, 0);
	styleset_set_style(sci, SCE_PROPS_COMMENT, GEANY_FILETYPES_CONF, 1);
	styleset_set_style(sci, SCE_PROPS_SECTION, GEANY_FILETYPES_CONF, 2);
	styleset_set_style(sci, SCE_PROPS_KEY, GEANY_FILETYPES_CONF, 3);
	styleset_set_style(sci, SCE_PROPS_ASSIGNMENT, GEANY_FILETYPES_CONF, 4);
	styleset_set_style(sci, SCE_PROPS_DEFVAL, GEANY_FILETYPES_CONF, 5);
}


static void styleset_asm_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.asm", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.asm", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_ASM] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "number", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "string", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x880000", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "cpuinstruction", "0x111199", "0xffffff", "true", types[GEANY_FILETYPES_ASM]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "mathinstruction", "0x7f0000", "0xffffff", "true", types[GEANY_FILETYPES_ASM]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "register", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "directive", "0x3d670f", "0xffffff", "true", types[GEANY_FILETYPES_ASM]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "directiveoperand", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "commentblock", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "character", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[12]);
	styleset_get_hex(config, config_home, "styling", "stringeol", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_ASM]->styling[13]);
	styleset_get_hex(config, config_home, "styling", "extinstruction", "0x007f7f", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[14]);

	types[GEANY_FILETYPES_ASM]->keywords = g_new(gchar*, 4);
	styleset_get_keywords(config, config_home, "keywords", "instructions", GEANY_FILETYPES_ASM, 0, "HLT LAD SPI ADD SUB MUL DIV JMP JEZ JGZ JLZ SWAP JSR RET PUSHAC POPAC ADDST SUBST MULST DIVST LSA LDS PUSH POP CLI LDI INK LIA DEK LDX");
	styleset_get_keywords(config, config_home, "keywords", "registers", GEANY_FILETYPES_ASM, 1, "");
	styleset_get_keywords(config, config_home, "keywords", "directives", GEANY_FILETYPES_ASM, 2, "ORG LIST NOLIST PAGE EQUIVALENT WORD TEXT");
	types[GEANY_FILETYPES_ASM]->keywords[3] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_ASM, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_ASM);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_asm(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_ASM] == NULL) styleset_asm_init();

	styleset_common(sci, 5);

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_ASM]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM(sci, SCI_SETLEXER, SCLEX_ASM, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_ASM]->keywords[0]);
	//SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) types[GEANY_FILETYPES_ASM]->keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) types[GEANY_FILETYPES_ASM]->keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 3, (sptr_t) types[GEANY_FILETYPES_ASM]->keywords[2]);
	//SSM(sci, SCI_SETKEYWORDS, 5, (sptr_t) types[GEANY_FILETYPES_ASM]->keywords[0]);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_ASM, 0);
	styleset_set_style(sci, SCE_ASM_DEFAULT, GEANY_FILETYPES_ASM, 0);
	styleset_set_style(sci, SCE_ASM_COMMENT, GEANY_FILETYPES_ASM, 1);
	styleset_set_style(sci, SCE_ASM_NUMBER, GEANY_FILETYPES_ASM, 2);
	styleset_set_style(sci, SCE_ASM_STRING, GEANY_FILETYPES_ASM, 3);
	styleset_set_style(sci, SCE_ASM_OPERATOR, GEANY_FILETYPES_ASM, 4);
	styleset_set_style(sci, SCE_ASM_IDENTIFIER, GEANY_FILETYPES_ASM, 5);
	styleset_set_style(sci, SCE_ASM_CPUINSTRUCTION, GEANY_FILETYPES_ASM, 6);
	styleset_set_style(sci, SCE_ASM_MATHINSTRUCTION, GEANY_FILETYPES_ASM, 7);
	styleset_set_style(sci, SCE_ASM_REGISTER, GEANY_FILETYPES_ASM, 8);
	styleset_set_style(sci, SCE_ASM_DIRECTIVE, GEANY_FILETYPES_ASM, 9);
	styleset_set_style(sci, SCE_ASM_DIRECTIVEOPERAND, GEANY_FILETYPES_ASM, 10);
	styleset_set_style(sci, SCE_ASM_COMMENTBLOCK, GEANY_FILETYPES_ASM, 11);
	styleset_set_style(sci, SCE_ASM_CHARACTER, GEANY_FILETYPES_ASM, 12);
	styleset_set_style(sci, SCE_ASM_STRINGEOL, GEANY_FILETYPES_ASM, 13);
	styleset_set_style(sci, SCE_ASM_EXTINSTRUCTION, GEANY_FILETYPES_ASM, 14);
}


static void styleset_sql_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.sql", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.sql", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_SQL] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_SQL]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_SQL]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "commentline", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_SQL]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "commentdoc", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_SQL]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "number", "0x7f7f00", "0xffffff", "false", types[GEANY_FILETYPES_SQL]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "word", "0x001a7f", "0xffffff", "true", types[GEANY_FILETYPES_SQL]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "word2", "0x7f0000", "0xffffff", "true", types[GEANY_FILETYPES_SQL]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "string", "0x7f007f", "0xffffff", "false", types[GEANY_FILETYPES_SQL]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "character", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_SQL]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x000000", "0xffffff", "true", types[GEANY_FILETYPES_SQL]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x111199", "0xffffff", "false", types[GEANY_FILETYPES_SQL]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "sqlplus", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_SQL]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "sqlplus_prompt", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_SQL]->styling[12]);
	styleset_get_hex(config, config_home, "styling", "sqlplus_comment", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_SQL]->styling[13]);
	styleset_get_hex(config, config_home, "styling", "quotedidentifier", "0x111199", "0xffffff", "false", types[GEANY_FILETYPES_SQL]->styling[14]);

	types[GEANY_FILETYPES_SQL]->keywords = g_new(gchar*, 2);
	styleset_get_keywords(config, config_home, "keywords", "keywords", GEANY_FILETYPES_SQL, 0,
						"absolute action add admin after aggregate \
						alias all allocate alter and any are array as asc \
						assertion at authorization before begin binary bit blob boolean both breadth by \
						call cascade cascaded case cast catalog char character check class clob close collate \
						collation column commit completion connect connection constraint constraints \
						constructor continue corresponding create cross cube current \
						current_date current_path current_role current_time current_timestamp \
						current_user cursor cycle data date day deallocate dec decimal declare default \
						deferrable deferred delete depth deref desc describe descriptor destroy destructor \
						deterministic dictionary diagnostics disconnect distinct domain double drop dynamic \
						each else end end-exec equals escape every except exception exec execute external \
						false fetch first float for foreign found from free full function general get global \
						go goto grant group grouping having host hour identity if ignore immediate in indicator \
						initialize initially inner inout input insert int integer intersect interval \
						into is isolation iterate join key language large last lateral leading left less level like \
						limit local localtime localtimestamp locator map match minute modifies modify module month \
						names national natural nchar nclob new next no none not null numeric object of off old on only \
						open operation option or order ordinality out outer output pad parameter parameters partial path \
						postfix precision prefix preorder prepare preserve primary prior privileges procedure public \
						read reads real recursive ref references referencing relative restrict result return returns \
						revoke right role rollback rollup routine row rows savepoint schema scroll scope search \
						second section select sequence session session_user set sets size smallint some space \
						specific specifictype sql sqlexception sqlstate sqlwarning start state statement static \
						structure system_user table temporary terminate than then time timestamp \
						timezone_hour timezone_minute to trailing transaction translation year zone\
						treat trigger true under union unique unknown unnest update usage user using \
						value values varchar variable varying view when whenever where with without work write");
	types[GEANY_FILETYPES_SQL]->keywords[1] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_SQL, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_SQL);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_sql(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_SQL] == NULL) styleset_sql_init();

	styleset_common(sci, 5);

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_SQL]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM(sci, SCI_SETLEXER, SCLEX_SQL, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_SQL]->keywords[0]);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_SQL, 0);
	styleset_set_style(sci, SCE_SQL_DEFAULT, GEANY_FILETYPES_SQL, 0);
	styleset_set_style(sci, SCE_SQL_COMMENT, GEANY_FILETYPES_SQL, 1);
	styleset_set_style(sci, SCE_SQL_COMMENTLINE, GEANY_FILETYPES_SQL, 2);
	styleset_set_style(sci, SCE_SQL_COMMENTDOC, GEANY_FILETYPES_SQL, 3);
	styleset_set_style(sci, SCE_SQL_NUMBER, GEANY_FILETYPES_SQL, 4);
	styleset_set_style(sci, SCE_SQL_WORD, GEANY_FILETYPES_SQL, 5);
	styleset_set_style(sci, SCE_SQL_WORD2, GEANY_FILETYPES_SQL, 6);
	styleset_set_style(sci, SCE_SQL_STRING, GEANY_FILETYPES_SQL, 7);
	styleset_set_style(sci, SCE_SQL_CHARACTER, GEANY_FILETYPES_SQL, 8);
	styleset_set_style(sci, SCE_SQL_OPERATOR, GEANY_FILETYPES_SQL, 9);
	styleset_set_style(sci, SCE_SQL_IDENTIFIER, GEANY_FILETYPES_SQL, 10);
	styleset_set_style(sci, SCE_SQL_SQLPLUS, GEANY_FILETYPES_SQL, 11);
	styleset_set_style(sci, SCE_SQL_SQLPLUS_PROMPT, GEANY_FILETYPES_SQL, 12);
	styleset_set_style(sci, SCE_SQL_SQLPLUS_COMMENT, GEANY_FILETYPES_SQL, 13);
	styleset_set_style(sci, SCE_SQL_QUOTEDIDENTIFIER, GEANY_FILETYPES_SQL, 14);
}


static void styleset_caml_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.caml", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.caml", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_CAML] = g_new(style_set, 1);

	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_CAML]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_CAML]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "comment1", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_CAML]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "comment2", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_CAML]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "comment3", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_CAML]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "number", "0x7f7f00", "0xffffff", "false", types[GEANY_FILETYPES_CAML]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "keyword", "0x001a7f", "0xffffff", "true", types[GEANY_FILETYPES_CAML]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "keyword2", "0x7f0000", "0xffffff", "true", types[GEANY_FILETYPES_CAML]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "string", "0x7f007f", "0xffffff", "false", types[GEANY_FILETYPES_CAML]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "char", "0x7f007f", "0xffffff", "false", types[GEANY_FILETYPES_CAML]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_CAML]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x111199", "0xffffff", "false", types[GEANY_FILETYPES_CAML]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "tagname", "0x000000", "0xffe0ff", "true", types[GEANY_FILETYPES_CAML]->styling[12]);
	styleset_get_hex(config, config_home, "styling", "linenum", "0x000000", "0xc0c0c0", "false", types[GEANY_FILETYPES_CAML]->styling[13]);

	types[GEANY_FILETYPES_CAML]->keywords = g_new(gchar*, 3);
	styleset_get_keywords(config, config_home, "keywords", "keywords", GEANY_FILETYPES_CAML, 0,
			"and as assert asr begin class constraint do \
			done downto else end exception external false for fun function functor if in include inherit \
			initializer land lazy let lor lsl lsr lxor match method mod module mutable new object of open \
			or private rec sig struct then to true try type val virtual when while with");
	styleset_get_keywords(config, config_home, "keywords", "keywords_optional", GEANY_FILETYPES_CAML, 1, "option Some None ignore ref");
	types[GEANY_FILETYPES_CAML]->keywords[2] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_CAML, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_CAML);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_caml(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_CAML] == NULL) styleset_caml_init();

	styleset_common(sci, 5);

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_CAML]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM(sci, SCI_SETLEXER, SCLEX_CAML, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_CAML]->keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) types[GEANY_FILETYPES_CAML]->keywords[1]);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_CAML, 0);
	styleset_set_style(sci, SCE_CAML_DEFAULT, GEANY_FILETYPES_CAML, 0);
	styleset_set_style(sci, SCE_CAML_COMMENT, GEANY_FILETYPES_CAML, 1);
	styleset_set_style(sci, SCE_CAML_COMMENT1, GEANY_FILETYPES_CAML, 2);
	styleset_set_style(sci, SCE_CAML_COMMENT2, GEANY_FILETYPES_CAML, 3);
	styleset_set_style(sci, SCE_CAML_COMMENT3, GEANY_FILETYPES_CAML, 4);
	styleset_set_style(sci, SCE_CAML_NUMBER, GEANY_FILETYPES_CAML, 5);
	styleset_set_style(sci, SCE_CAML_KEYWORD, GEANY_FILETYPES_CAML, 6);
	styleset_set_style(sci, SCE_CAML_KEYWORD2, GEANY_FILETYPES_CAML, 7);
	styleset_set_style(sci, SCE_CAML_STRING, GEANY_FILETYPES_CAML, 8);
	styleset_set_style(sci, SCE_CAML_CHAR, GEANY_FILETYPES_CAML, 9);
	styleset_set_style(sci, SCE_CAML_OPERATOR, GEANY_FILETYPES_CAML, 10);
	styleset_set_style(sci, SCE_CAML_IDENTIFIER, GEANY_FILETYPES_CAML, 11);
	styleset_set_style(sci, SCE_CAML_TAGNAME, GEANY_FILETYPES_CAML, 12);
	styleset_set_style(sci, SCE_CAML_LINENUM, GEANY_FILETYPES_CAML, 13);
}


static void styleset_oms_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.oms", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.oms", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_OMS] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_OMS]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "commentline", "0x909090", "0xffffff", "false", types[GEANY_FILETYPES_OMS]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "number", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_OMS]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "word", "0x991111", "0xffffff", "false", types[GEANY_FILETYPES_OMS]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "string", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_OMS]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "character", "0x404000", "0xffffff", "false", types[GEANY_FILETYPES_OMS]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_OMS]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_OMS]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "backticks", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_OMS]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "param", "0x991111", "0x0000ff", "false", types[GEANY_FILETYPES_OMS]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "scalar", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_OMS]->styling[10]);

	types[GEANY_FILETYPES_OMS]->keywords = g_new(gchar*, 2);
	styleset_get_keywords(config, config_home, "keywords", "primary", GEANY_FILETYPES_OMS, 0, "clear seq fillcols fillrowsgaspect gaddview \
			gtitle gxaxis gyaxis max contour gcolor gplot gaddview gxaxis gyaxis gcolor fill coldim gplot \
			gtitle clear arcov dpss fspec cos gxaxis gyaxis gtitle gplot gupdate rowdim fill print for to begin \
			end write cocreate coinvoke codispsave cocreate codispset copropput colsum sqrt adddialog \
			addcontrol addcontrol delwin fillrows function gaspect conjdir");
	types[GEANY_FILETYPES_OMS]->keywords[1] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_OMS, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_OMS);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_oms(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_OMS] == NULL) styleset_oms_init();

	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_OMS, 0);

	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_OMS]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM (sci, SCI_SETCONTROLCHARSYMBOL, 32, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_OMS]->keywords[0]);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_OMS, 0);
	styleset_set_style(sci, SCE_SH_DEFAULT, GEANY_FILETYPES_OMS, 0);
	styleset_set_style(sci, SCE_SH_COMMENTLINE, GEANY_FILETYPES_OMS, 1);
	styleset_set_style(sci, SCE_SH_NUMBER, GEANY_FILETYPES_OMS, 2);
	styleset_set_style(sci, SCE_SH_WORD, GEANY_FILETYPES_OMS, 3);
	styleset_set_style(sci, SCE_SH_STRING, GEANY_FILETYPES_OMS, 4);
	styleset_set_style(sci, SCE_SH_CHARACTER, GEANY_FILETYPES_OMS, 5);
	styleset_set_style(sci, SCE_SH_OPERATOR, GEANY_FILETYPES_OMS, 6);
	styleset_set_style(sci, SCE_SH_IDENTIFIER, GEANY_FILETYPES_OMS, 7);
	styleset_set_style(sci, SCE_SH_BACKTICKS, GEANY_FILETYPES_OMS, 8);
	styleset_set_style(sci, SCE_SH_PARAM, GEANY_FILETYPES_OMS, 9);
	styleset_set_style(sci, SCE_SH_SCALAR, GEANY_FILETYPES_OMS, 10);
}


static void styleset_tcl_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.tcl", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.tcl", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_TCL] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_TCL]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "comment", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_TCL]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "commentline", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_TCL]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "number", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_TCL]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_TCL]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0xa20000", "0xffffff", "false", types[GEANY_FILETYPES_TCL]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "wordinquote", "0x7f007f", "0xffffff", "false", types[GEANY_FILETYPES_TCL]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "inquote", "0x7f007f", "0xffffff", "false", types[GEANY_FILETYPES_TCL]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "substitution", "0x111199", "0xffffff", "false", types[GEANY_FILETYPES_TCL]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "modifier", "0x7f007f", "0xffffff", "false", types[GEANY_FILETYPES_TCL]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "expand", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_TCL]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "wordtcl", "0x111199", "0xffffff", "true", types[GEANY_FILETYPES_TCL]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "wordtk", "0x7f0000", "0xffffff", "true", types[GEANY_FILETYPES_TCL]->styling[12]);
	styleset_get_hex(config, config_home, "styling", "worditcl", "0x111199", "0xffffff", "true", types[GEANY_FILETYPES_TCL]->styling[13]);
	styleset_get_hex(config, config_home, "styling", "wordtkcmds", "0x7f0000", "0xffffff", "true", types[GEANY_FILETYPES_TCL]->styling[14]);
	styleset_get_hex(config, config_home, "styling", "wordexpand", "0x7f0000", "0xffffff", "true", types[GEANY_FILETYPES_TCL]->styling[15]);

	types[GEANY_FILETYPES_TCL]->keywords = g_new(gchar*, 6);
	styleset_get_keywords(config, config_home, "keywords", "tcl", GEANY_FILETYPES_TCL, 0, "");
	styleset_get_keywords(config, config_home, "keywords", "tk", GEANY_FILETYPES_TCL, 1, "");
	styleset_get_keywords(config, config_home, "keywords", "itcl", GEANY_FILETYPES_TCL, 2, "");
	styleset_get_keywords(config, config_home, "keywords", "tkcommands", GEANY_FILETYPES_TCL, 3, "");
	styleset_get_keywords(config, config_home, "keywords", "expand", GEANY_FILETYPES_TCL, 4, "");
	types[GEANY_FILETYPES_TCL]->keywords[5] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_TCL, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_TCL);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);
}


void styleset_tcl(ScintillaObject *sci)
{

	if (types[GEANY_FILETYPES_TCL] == NULL) styleset_tcl_init();

	styleset_common(sci, 5);


	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_TCL]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM(sci, SCI_SETLEXER, SCLEX_TCL, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_TCL]->keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) types[GEANY_FILETYPES_TCL]->keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) types[GEANY_FILETYPES_TCL]->keywords[2]);
	SSM(sci, SCI_SETKEYWORDS, 3, (sptr_t) types[GEANY_FILETYPES_TCL]->keywords[3]);
	SSM(sci, SCI_SETKEYWORDS, 4, (sptr_t) types[GEANY_FILETYPES_TCL]->keywords[4]);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_TCL, 0);
	styleset_set_style(sci, SCE_TCL_DEFAULT, GEANY_FILETYPES_TCL, 0);
	styleset_set_style(sci, SCE_TCL_COMMENT, GEANY_FILETYPES_TCL, 1);
	styleset_set_style(sci, SCE_TCL_COMMENTLINE, GEANY_FILETYPES_TCL, 2);
	styleset_set_style(sci, SCE_TCL_NUMBER, GEANY_FILETYPES_TCL, 3);
	styleset_set_style(sci, SCE_TCL_OPERATOR, GEANY_FILETYPES_TCL, 4);
	styleset_set_style(sci, SCE_TCL_IDENTIFIER, GEANY_FILETYPES_TCL, 5);
	styleset_set_style(sci, SCE_TCL_WORD_IN_QUOTE, GEANY_FILETYPES_TCL, 6);
	styleset_set_style(sci, SCE_TCL_IN_QUOTE, GEANY_FILETYPES_TCL, 7);
	styleset_set_style(sci, SCE_TCL_SUBSTITUTION, GEANY_FILETYPES_TCL, 8);
	styleset_set_style(sci, SCE_TCL_MODIFIER, GEANY_FILETYPES_TCL, 9);
	styleset_set_style(sci, SCE_TCL_EXPAND, GEANY_FILETYPES_TCL, 10);
	styleset_set_style(sci, SCE_TCL_WORD, GEANY_FILETYPES_TCL, 11);
	styleset_set_style(sci, SCE_TCL_WORD2, GEANY_FILETYPES_TCL, 12);
	styleset_set_style(sci, SCE_TCL_WORD3, GEANY_FILETYPES_TCL, 13);
	styleset_set_style(sci, SCE_TCL_WORD4, GEANY_FILETYPES_TCL, 14);
	styleset_set_style(sci, SCE_TCL_WORD5, GEANY_FILETYPES_TCL, 15);
}

static void styleset_d_init(void)
{
	GKeyFile *config = g_key_file_new();
	GKeyFile *config_home = g_key_file_new();
	gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.d", NULL);
	gchar *f = g_strconcat(app->configdir, G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.d", NULL);

	styleset_load_file(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
	g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_D] = g_new(style_set, 1);
	styleset_get_hex(config, config_home, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_D]->styling[0]);
	styleset_get_hex(config, config_home, "styling", "comment", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_D]->styling[1]);
	styleset_get_hex(config, config_home, "styling", "commentline", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_D]->styling[2]);
	styleset_get_hex(config, config_home, "styling", "commentdoc", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_D]->styling[3]);
	styleset_get_hex(config, config_home, "styling", "number", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_D]->styling[4]);
	styleset_get_hex(config, config_home, "styling", "word", "0x111199", "0xffffff", "true", types[GEANY_FILETYPES_D]->styling[5]);
	styleset_get_hex(config, config_home, "styling", "word2", "0x7f0000", "0xffffff", "true", types[GEANY_FILETYPES_D]->styling[6]);
	styleset_get_hex(config, config_home, "styling", "string", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_D]->styling[7]);
	styleset_get_hex(config, config_home, "styling", "character", "0xff901e", "0xffffff", "false", types[GEANY_FILETYPES_D]->styling[8]);
	styleset_get_hex(config, config_home, "styling", "uuid", "0x404080", "0xffffff", "false", types[GEANY_FILETYPES_D]->styling[9]);
	styleset_get_hex(config, config_home, "styling", "preprocessor", "0x007f7f", "0xffffff", "false", types[GEANY_FILETYPES_D]->styling[10]);
	styleset_get_hex(config, config_home, "styling", "operator", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_D]->styling[11]);
	styleset_get_hex(config, config_home, "styling", "identifier", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_D]->styling[12]);
	styleset_get_hex(config, config_home, "styling", "stringeol", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_D]->styling[13]);
	styleset_get_hex(config, config_home, "styling", "verbatim", "0x301010", "0xffffff", "false", types[GEANY_FILETYPES_D]->styling[14]);
	styleset_get_hex(config, config_home, "styling", "regex", "0x105090", "0xffffff", "false", types[GEANY_FILETYPES_D]->styling[15]);
	styleset_get_hex(config, config_home, "styling", "commentlinedoc", "0xff0000", "0xffffff", "true", types[GEANY_FILETYPES_D]->styling[16]);
	styleset_get_hex(config, config_home, "styling", "commentdockeyword", "0xff0000", "0xffffff", "true", types[GEANY_FILETYPES_D]->styling[17]);
	styleset_get_hex(config, config_home, "styling", "globalclass", "0x1111bb", "0xffffff", "true", types[GEANY_FILETYPES_D]->styling[18]);
	styleset_get_int(config, config_home, "styling", "styling_within_preprocessor", 1, 0, types[GEANY_FILETYPES_D]->styling[19]);

	types[GEANY_FILETYPES_D]->keywords = g_new(gchar*, 3);
	styleset_get_keywords(config, config_home, "keywords", "primary", GEANY_FILETYPES_D, 0, "__FILE__ __LINE__ __DATA__ __TIME__ __TIMESTAMP__ abstract alias align asm assert auto body bool break byte case cast catch cdouble cent cfloat char class const continue creal dchar debug default delegate delete deprecated do double else enum export extern false final finally float for foreach function goto idouble if ifloat import in inout int interface invariant ireal is long mixin module new null out override package pragma private protected public real return scope short static struct super switch synchronized template this throw true try typedef typeof ubyte ucent uint ulong union unittest ushort version void volatile wchar while with");
	styleset_get_keywords(config, config_home, "keywords", "docComment", GEANY_FILETYPES_D, 1, "TODO FIXME");
	types[GEANY_FILETYPES_D]->keywords[2] = NULL;

	styleset_get_wordchars(config, config_home, GEANY_FILETYPES_D, GEANY_WORDCHARS);
	filetypes_get_config(config, config_home, GEANY_FILETYPES_D);

	g_key_file_free(config);
	g_key_file_free(config_home);
	g_free(f0);
	g_free(f);

	// load global tags file for C autocompletion
	if (! app->ignore_global_tags && ! global_c_tags_loaded)
	{
		gchar *file = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "global.tags", NULL);
		// 0 is the langType used in TagManager (see the table in tagmanager/parsers.h)
		// C++ is a special case, here we use 0 to have C global tags in C++, too
		tm_workspace_load_global_tags(file, 0);
		global_c_tags_loaded = TRUE;
		g_free(file);
	}
}


void styleset_d(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_D] == NULL) styleset_d_init();

	styleset_common(sci, 5);

	/* Assign global keywords */
	if ((app->tm_workspace) && (app->tm_workspace->global_tags))
	{
		guint j;
		GPtrArray *g_typedefs = tm_tags_extract(app->tm_workspace->global_tags, tm_tag_typedef_t | tm_tag_struct_t | tm_tag_class_t);
		if ((g_typedefs) && (g_typedefs->len > 0))
		{
			GString *s = g_string_sized_new(g_typedefs->len * 10);
			for (j = 0; j < g_typedefs->len; ++j)
			{
				if (!(TM_TAG(g_typedefs->pdata[j])->atts.entry.scope))
				{
					g_string_append(s, TM_TAG(g_typedefs->pdata[j])->name);
					g_string_append_c(s, ' ');
				}
			}
			SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) s->str);
			g_string_free(s, TRUE);
		}
		g_ptr_array_free(g_typedefs, TRUE);
	}

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) types[GEANY_FILETYPES_D]->wordchars);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, app->autocompletion_max_height, 0);

	SSM(sci, SCI_SETLEXER, SCLEX_CPP, 0);

	//SSM(sci, SCI_SETCONTROLCHARSYMBOL, 32, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_D]->keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) types[GEANY_FILETYPES_D]->keywords[1]);

	styleset_set_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_D, 0);
	styleset_set_style(sci, SCE_C_DEFAULT, GEANY_FILETYPES_D, 0);
	styleset_set_style(sci, SCE_C_COMMENT, GEANY_FILETYPES_D, 1);
	styleset_set_style(sci, SCE_C_COMMENTLINE, GEANY_FILETYPES_D, 2);
	styleset_set_style(sci, SCE_C_COMMENTDOC, GEANY_FILETYPES_D, 3);
	styleset_set_style(sci, SCE_C_NUMBER, GEANY_FILETYPES_D, 4);
	styleset_set_style(sci, SCE_C_WORD, GEANY_FILETYPES_D, 5);
	styleset_set_style(sci, SCE_C_WORD2, GEANY_FILETYPES_D, 6);
	styleset_set_style(sci, SCE_C_STRING, GEANY_FILETYPES_D, 7);
	styleset_set_style(sci, SCE_C_CHARACTER, GEANY_FILETYPES_D, 8);
	styleset_set_style(sci, SCE_C_UUID, GEANY_FILETYPES_D, 9);
	styleset_set_style(sci, SCE_C_PREPROCESSOR, GEANY_FILETYPES_D, 10);
	styleset_set_style(sci, SCE_C_OPERATOR, GEANY_FILETYPES_D, 11);
	styleset_set_style(sci, SCE_C_IDENTIFIER, GEANY_FILETYPES_D, 12);
	styleset_set_style(sci, SCE_C_STRINGEOL, GEANY_FILETYPES_D, 13);
	styleset_set_style(sci, SCE_C_VERBATIM, GEANY_FILETYPES_D, 14);
	styleset_set_style(sci, SCE_C_REGEX, GEANY_FILETYPES_D, 15);
	styleset_set_style(sci, SCE_C_COMMENTLINEDOC, GEANY_FILETYPES_D, 16);
	styleset_set_style(sci, SCE_C_COMMENTDOCKEYWORD, GEANY_FILETYPES_D, 17);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTDOCKEYWORDERROR, invert(0x0000ff));
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTDOCKEYWORDERROR, invert(0xffffff));
	SSM(sci, SCI_STYLESETITALIC, SCE_C_COMMENTDOCKEYWORDERROR, TRUE);

	// is used for local structs and typedefs
	styleset_set_style(sci, SCE_C_GLOBALCLASS, GEANY_FILETYPES_D, 18);
}

