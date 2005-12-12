/*
 *      highligting.c - this file is part of Geany, a fast and lightweight IDE
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
 * $Id$
 */

#include <stdlib.h>

#include "highlighting.h"
#include "utils.h"


static style_set *types[GEANY_MAX_FILE_TYPES] = { NULL };


/* simple wrapper function to print file errors in DEBUG mode */
void style_set_load_file(GKeyFile *key_file, const gchar *file, GKeyFileFlags flags, GError **just_for_compatibility)
{
	GError *error = NULL;
	gboolean done = g_key_file_load_from_file(key_file, file, flags, &error);
	if (! done && error != NULL)
	{
		geany_debug("Failed to open %s (%s)", file, error->message);
		g_error_free(error);
	}
}


gchar *styleset_get_string(GKeyFile *config, const gchar *section, const gchar *key)
{
	GError *error = NULL;
	gchar *result;

	if (config == NULL || section == NULL) return NULL;

	result = g_key_file_get_string(config, section, key, &error);
	//if (error) geany_debug(error->message);

	return result;
}


void styleset_get_hex(GKeyFile *config, const gchar *section, const gchar *key,
					  const gchar *foreground, const gchar *background, const gchar *bold, gint array[])
{
	GError *error = NULL;
	gchar **list;
	gsize len;

	if (config == NULL || section == NULL) return;

	list = g_key_file_get_string_list(config, section, key, &len, &error);

	if (list != NULL && list[0] != NULL) array[0] = (gint) strtod(list[0], NULL);
	else array[0] = (gint) strtod(foreground, NULL);
	if (list != NULL && list[1] != NULL) array[1] = (gint) strtod(list[1], NULL);
	else array[1] = (gint) strtod(background, NULL);
	if (list != NULL && list[2] != NULL) array[2] = utils_atob(list[2]);
	else array[2] = utils_atob(bold);

	g_strfreev(list);
}


void styleset_free_styles()
{
	gint i;

	for (i = 0; i < GEANY_MAX_FILE_TYPES; i++)
	{
		if (types[i] != NULL)
		{
			g_strfreev(types[i]->keywords);
			g_free(types[i]);
		}
	}
}


void styleset_common_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.common", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_ALL] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_ALL]->styling[0]);
	styleset_get_hex(config, "styling", "selection", "0xc0c0c0", "0x00007f", "false", types[GEANY_FILETYPES_ALL]->styling[1]);
	styleset_get_hex(config, "styling", "brace_good", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_ALL]->styling[2]);
	styleset_get_hex(config, "styling", "brace_bad", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_ALL]->styling[3]);

	types[GEANY_FILETYPES_ALL]->keywords = NULL;

	g_key_file_free(config);
}


void styleset_common(ScintillaObject *sci, gint style_bits)
{
	if (types[GEANY_FILETYPES_ALL] == NULL) styleset_common_init();

	SSM(sci, SCI_STYLESETFORE, STYLE_DEFAULT, types[GEANY_FILETYPES_ALL]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, STYLE_DEFAULT, types[GEANY_FILETYPES_ALL]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, STYLE_DEFAULT, types[GEANY_FILETYPES_ALL]->styling[0][2]);

	SSM(sci, SCI_STYLECLEARALL, 0, 0);

	SSM(sci, SCI_SETUSETABS, TRUE, 0);
	SSM(sci, SCI_SETTABWIDTH, app->pref_editor_tab_width, 0);

	// colourize the current line
	SSM(sci, SCI_SETCARETLINEBACK, 0xE5E5E5, 0);
	SSM(sci, SCI_SETCARETLINEVISIBLE, 1, 0);

	// a darker grey for the line number margin
	SSM(sci, SCI_STYLESETBACK, STYLE_LINENUMBER, 0xD0D0D0);

	// define marker symbols
	// 0 -> line marker
	SSM(sci, SCI_MARKERDEFINE, 0, SC_MARK_SHORTARROW);
	SSM(sci, SCI_MARKERSETFORE, 0, 0x00007f);
	SSM(sci, SCI_MARKERSETBACK, 0, 0x00ffff);

	// 1 -> user marker
	SSM(sci, SCI_MARKERDEFINE, 1, SC_MARK_PLUS);
	SSM(sci, SCI_MARKERSETFORE, 1, 0x000000);
	SSM(sci, SCI_MARKERSETBACK, 1, 0xB8F4B8);

	SSM(sci, SCI_SETSELFORE, 1, types[GEANY_FILETYPES_ALL]->styling[1][0]);
	SSM(sci, SCI_SETSELBACK, 1, types[GEANY_FILETYPES_ALL]->styling[1][1]);

	SSM (sci, SCI_SETSTYLEBITS, style_bits, 0);

	SSM(sci, SCI_STYLESETFORE, STYLE_BRACELIGHT, types[GEANY_FILETYPES_ALL]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, STYLE_BRACELIGHT, types[GEANY_FILETYPES_ALL]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, STYLE_BRACELIGHT, types[GEANY_FILETYPES_ALL]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, STYLE_BRACEBAD, types[GEANY_FILETYPES_ALL]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, STYLE_BRACEBAD, types[GEANY_FILETYPES_ALL]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, STYLE_BRACEBAD, types[GEANY_FILETYPES_ALL]->styling[3][2]);
}


void styleset_c_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.c", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_C] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[0]);
	styleset_get_hex(config, "styling", "comment", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[1]);
	styleset_get_hex(config, "styling", "commentline", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[2]);
	styleset_get_hex(config, "styling", "commentdoc", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[3]);
	styleset_get_hex(config, "styling", "number", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[4]);
	styleset_get_hex(config, "styling", "word", "0x991111", "0xffffff", "true", types[GEANY_FILETYPES_C]->styling[5]);
	styleset_get_hex(config, "styling", "word2", "0x00007F", "0xffffff", "true", types[GEANY_FILETYPES_C]->styling[6]);
	styleset_get_hex(config, "styling", "string", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[7]);
	styleset_get_hex(config, "styling", "character", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[8]);
	styleset_get_hex(config, "styling", "uuid", "0x804040", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[9]);
	styleset_get_hex(config, "styling", "preprocessor", "0x7F7F00", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[10]);
	styleset_get_hex(config, "styling", "operator", "0x101030", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[11]);
	styleset_get_hex(config, "styling", "identifier", "0x100000", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[12]);
	styleset_get_hex(config, "styling", "stringeol", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_C]->styling[13]);
	styleset_get_hex(config, "styling", "verbatim", "0x101030", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[14]);
	styleset_get_hex(config, "styling", "regex", "0x101030", "0xffffff", "false", types[GEANY_FILETYPES_C]->styling[15]);
	styleset_get_hex(config, "styling", "commentlinedoc", "0x0000ff", "0xffffff", "true", types[GEANY_FILETYPES_C]->styling[16]);
	styleset_get_hex(config, "styling", "commentdockeyword", "0x0000ff", "0xffffff", "true", types[GEANY_FILETYPES_C]->styling[17]);
	styleset_get_hex(config, "styling", "globalclass", "0xbb1111", "0xffffff", "true", types[GEANY_FILETYPES_C]->styling[18]);

	types[GEANY_FILETYPES_C]->keywords = g_new(gchar*, 3);
	types[GEANY_FILETYPES_C]->keywords[0] = styleset_get_string(config, "keywords", "primary");
	if (types[GEANY_FILETYPES_C]->keywords[0] == NULL)
			types[GEANY_FILETYPES_C]->keywords[0] = g_strdup("if const struct char int float double void long for while do case switch return");
	types[GEANY_FILETYPES_C]->keywords[1] = styleset_get_string(config, "keywords", "docComment");
	if (types[GEANY_FILETYPES_C]->keywords[1] == NULL)
			types[GEANY_FILETYPES_C]->keywords[1] = g_strdup("TODO FIXME");
	types[GEANY_FILETYPES_C]->keywords[2] = NULL;

	g_key_file_free(config);

	// load global tags file for C autocompletion
	if (! app->ignore_global_tags)	tm_workspace_load_global_tags(GEANY_DATA_DIR "/global.tags");
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

	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) GEANY_WORDCHARS);
	SSM (sci, SCI_AUTOCSETMAXHEIGHT, 8, 0);

	SSM (sci, SCI_SETLEXER, SCLEX_CPP, 0);

	//SSM (sci, SCI_SETCONTROLCHARSYMBOL, 32, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_C]->keywords[0]);
	//SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) secondaryKeyWords);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) types[GEANY_FILETYPES_C]->keywords[1]);
	//SSM(sci, SCI_SETKEYWORDS, 4, (sptr_t) typedefsKeyWords);

	SSM(sci, SCI_STYLESETFORE, SCE_C_DEFAULT, types[GEANY_FILETYPES_C]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_DEFAULT, types[GEANY_FILETYPES_C]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_DEFAULT, types[GEANY_FILETYPES_C]->styling[0][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENT, types[GEANY_FILETYPES_C]->styling[1][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENT, types[GEANY_FILETYPES_C]->styling[1][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_COMMENT, types[GEANY_FILETYPES_C]->styling[1][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTLINE, types[GEANY_FILETYPES_C]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTLINE, types[GEANY_FILETYPES_C]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_COMMENTLINE, types[GEANY_FILETYPES_C]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTDOC, types[GEANY_FILETYPES_C]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTDOC, types[GEANY_FILETYPES_C]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_COMMENTDOC, types[GEANY_FILETYPES_C]->styling[3][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_NUMBER, types[GEANY_FILETYPES_C]->styling[4][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_NUMBER, types[GEANY_FILETYPES_C]->styling[4][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_NUMBER, types[GEANY_FILETYPES_C]->styling[4][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_WORD, types[GEANY_FILETYPES_C]->styling[5][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_WORD, types[GEANY_FILETYPES_C]->styling[5][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_WORD, types[GEANY_FILETYPES_C]->styling[5][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_WORD2, types[GEANY_FILETYPES_C]->styling[6][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_WORD2, types[GEANY_FILETYPES_C]->styling[6][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_WORD2, types[GEANY_FILETYPES_C]->styling[6][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_STRING, types[GEANY_FILETYPES_C]->styling[7][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_STRING, types[GEANY_FILETYPES_C]->styling[7][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_STRING, types[GEANY_FILETYPES_C]->styling[7][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_CHARACTER, types[GEANY_FILETYPES_C]->styling[8][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_CHARACTER, types[GEANY_FILETYPES_C]->styling[8][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_CHARACTER, types[GEANY_FILETYPES_C]->styling[8][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_UUID, types[GEANY_FILETYPES_C]->styling[9][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_UUID, types[GEANY_FILETYPES_C]->styling[9][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_UUID, types[GEANY_FILETYPES_C]->styling[9][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_PREPROCESSOR, types[GEANY_FILETYPES_C]->styling[10][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_PREPROCESSOR, types[GEANY_FILETYPES_C]->styling[10][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_PREPROCESSOR, types[GEANY_FILETYPES_C]->styling[10][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_OPERATOR, types[GEANY_FILETYPES_C]->styling[11][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_OPERATOR, types[GEANY_FILETYPES_C]->styling[11][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_OPERATOR, types[GEANY_FILETYPES_C]->styling[11][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_IDENTIFIER, types[GEANY_FILETYPES_C]->styling[12][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_IDENTIFIER, types[GEANY_FILETYPES_C]->styling[12][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_IDENTIFIER, types[GEANY_FILETYPES_C]->styling[12][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_STRINGEOL, types[GEANY_FILETYPES_C]->styling[13][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_STRINGEOL, types[GEANY_FILETYPES_C]->styling[13][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_STRINGEOL, types[GEANY_FILETYPES_C]->styling[13][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_VERBATIM, types[GEANY_FILETYPES_C]->styling[14][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_VERBATIM, types[GEANY_FILETYPES_C]->styling[14][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_VERBATIM, types[GEANY_FILETYPES_C]->styling[14][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_REGEX, types[GEANY_FILETYPES_C]->styling[15][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_REGEX, types[GEANY_FILETYPES_C]->styling[15][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_REGEX, types[GEANY_FILETYPES_C]->styling[15][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTLINEDOC, types[GEANY_FILETYPES_C]->styling[16][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTLINEDOC, types[GEANY_FILETYPES_C]->styling[16][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_COMMENTLINEDOC, types[GEANY_FILETYPES_C]->styling[16][2]);
	SSM(sci, SCI_STYLESETITALIC, SCE_C_COMMENTLINEDOC, TRUE);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTDOCKEYWORD, types[GEANY_FILETYPES_C]->styling[17][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTDOCKEYWORD, types[GEANY_FILETYPES_C]->styling[17][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_COMMENTDOCKEYWORD, types[GEANY_FILETYPES_C]->styling[17][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTDOCKEYWORDERROR, 0x0000ff);
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTDOCKEYWORDERROR, 0xFFFFFF);
	SSM(sci, SCI_STYLESETITALIC, SCE_C_COMMENTDOCKEYWORDERROR, TRUE);

	// is used for local structs and typedefs
	SSM(sci, SCI_STYLESETFORE, SCE_C_GLOBALCLASS, types[GEANY_FILETYPES_C]->styling[18][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_GLOBALCLASS, types[GEANY_FILETYPES_C]->styling[18][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_GLOBALCLASS, types[GEANY_FILETYPES_C]->styling[18][2]);

	SSM(sci, SCI_SETWHITESPACEFORE, 1, 0xc0c0c0);

	// I dont like/need folding
	//SSM (sci, SCI_SETPROPERTY, (sptr_t) "fold.preprocessor", (sptr_t) "1");
	//SSM (sci, SCI_SETPROPERTY, (sptr_t) "fold.at.else", (sptr_t) "1");

	// enable folding for retrieval of current function (utils_get_current_tag)
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "fold", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "styling.within.preprocessor", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "file.patterns.cpp", (sptr_t) "*.cpp;*.cxx;*.cc");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "preprocessor.symbol.$(file.patterns.cpp)", (sptr_t) "#");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "preprocessor.start.$(file.patterns.cpp)", (sptr_t) "if ifdef ifndef");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "preprocessor.middle.$(file.patterns.cpp)", (sptr_t) "else elif");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "preprocessor.end.$(file.patterns.cpp)", (sptr_t) "endif");

	//SSM(sci, SCI_SETPROPERTY, (sptr_t) "statement.indent.$(file.patterns.cpp)", (sptr_t) "5 case catch class default do else for if private protected public struct try union while switch");
	//SSM(sci, SCI_SETPROPERTY, (sptr_t) "statement.indent.$(file.patterns.header)", (sptr_t) "5 case catch class default do else for if private protected public struct try union while switch");
	//SSM(sci, SCI_SETPROPERTY, (sptr_t) "statement.indent.$(file.patterns.c)", (sptr_t) "5 case do else for if struct union while switch");
}


void styleset_pascal_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.pascal", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_PASCAL] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "default", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[0]);
	styleset_get_hex(config, "styling", "comment", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[1]);
	styleset_get_hex(config, "styling", "number", "0x007F00", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[2]);
	styleset_get_hex(config, "styling", "word", "0x991111", "0xffffff", "true", types[GEANY_FILETYPES_PASCAL]->styling[3]);
	styleset_get_hex(config, "styling", "string", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[4]);
	styleset_get_hex(config, "styling", "character", "0x004040", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[5]);
	styleset_get_hex(config, "styling", "preprocessor", "0x7F7F00", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[6]);
	styleset_get_hex(config, "styling", "operator", "0x101030", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[7]);
	styleset_get_hex(config, "styling", "identifier", "0x100000", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[8]);
	styleset_get_hex(config, "styling", "regex", "0x13631B", "0xffffff", "false", types[GEANY_FILETYPES_PASCAL]->styling[9]);

	types[GEANY_FILETYPES_PASCAL]->keywords = g_new(gchar*, 2);
	types[GEANY_FILETYPES_PASCAL]->keywords[0] = styleset_get_string(config, "keywords", "primary");
	if (types[GEANY_FILETYPES_PASCAL]->keywords[0] == NULL)
			types[GEANY_FILETYPES_PASCAL]->keywords[0] = g_strdup("word integer char string byte real \
									for to do until repeat program if uses then else case var begin end \
									asm unit interface implementation procedure function object try class");
	types[GEANY_FILETYPES_PASCAL]->keywords[1] = NULL;

	g_key_file_free(config);
}


void styleset_pascal(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_PASCAL] == NULL) styleset_pascal_init();

	styleset_common(sci, 5);

	// enable folding for retrieval of current function (utils_get_current_tag)
	SSM (sci, SCI_SETPROPERTY, (sptr_t) "fold", (sptr_t) "1");

	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) GEANY_WORDCHARS);
	SSM (sci, SCI_AUTOCSETMAXHEIGHT, 8, 0);

	SSM (sci, SCI_SETLEXER, SCLEX_PASCAL, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_PASCAL]->keywords[0]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_DEFAULT, types[GEANY_FILETYPES_PASCAL]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_DEFAULT, types[GEANY_FILETYPES_PASCAL]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_DEFAULT, types[GEANY_FILETYPES_PASCAL]->styling[0][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENT, types[GEANY_FILETYPES_PASCAL]->styling[1][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENT, types[GEANY_FILETYPES_PASCAL]->styling[1][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_COMMENT, types[GEANY_FILETYPES_PASCAL]->styling[1][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_NUMBER, types[GEANY_FILETYPES_PASCAL]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_NUMBER, types[GEANY_FILETYPES_PASCAL]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_NUMBER, types[GEANY_FILETYPES_PASCAL]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_WORD, types[GEANY_FILETYPES_PASCAL]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_WORD, types[GEANY_FILETYPES_PASCAL]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_WORD, types[GEANY_FILETYPES_PASCAL]->styling[3][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_STRING, types[GEANY_FILETYPES_PASCAL]->styling[4][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_STRING, types[GEANY_FILETYPES_PASCAL]->styling[4][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_STRING, types[GEANY_FILETYPES_PASCAL]->styling[4][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_CHARACTER, types[GEANY_FILETYPES_PASCAL]->styling[5][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_CHARACTER, types[GEANY_FILETYPES_PASCAL]->styling[5][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_CHARACTER, types[GEANY_FILETYPES_PASCAL]->styling[5][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_PREPROCESSOR, types[GEANY_FILETYPES_PASCAL]->styling[6][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_PREPROCESSOR, types[GEANY_FILETYPES_PASCAL]->styling[6][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_PREPROCESSOR, types[GEANY_FILETYPES_PASCAL]->styling[6][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_OPERATOR, types[GEANY_FILETYPES_PASCAL]->styling[7][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_OPERATOR, types[GEANY_FILETYPES_PASCAL]->styling[7][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_OPERATOR, types[GEANY_FILETYPES_PASCAL]->styling[7][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_IDENTIFIER, types[GEANY_FILETYPES_PASCAL]->styling[8][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_IDENTIFIER, types[GEANY_FILETYPES_PASCAL]->styling[8][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_IDENTIFIER, types[GEANY_FILETYPES_PASCAL]->styling[8][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_REGEX, types[GEANY_FILETYPES_PASCAL]->styling[9][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_REGEX, types[GEANY_FILETYPES_PASCAL]->styling[9][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_REGEX, types[GEANY_FILETYPES_PASCAL]->styling[9][2]);

	SSM(sci, SCI_SETWHITESPACEFORE, 1, 0xc0c0c0);

	//SSM(sci, SCI_SETPROPERTY, (sptr_t) "styling.within.preprocessor", (sptr_t) "1");

}


void styleset_makefile_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.makefile", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_MAKE] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "default", "0xFF0000", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[0]);
	styleset_get_hex(config, "styling", "comment", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[1]);
	styleset_get_hex(config, "styling", "preprocessor", "0x7F7F00", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[2]);
	styleset_get_hex(config, "styling", "identifier", "0x007F00", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[3]);
	styleset_get_hex(config, "styling", "operator", "0x101030", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[4]);
	styleset_get_hex(config, "styling", "target", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[5]);
	styleset_get_hex(config, "styling", "ideol", "0x008000", "0xffffff", "false", types[GEANY_FILETYPES_MAKE]->styling[6]);

	types[GEANY_FILETYPES_MAKE]->keywords = NULL;

	g_key_file_free(config);
}


void styleset_makefile(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_MAKE] == NULL) styleset_makefile_init();

	styleset_common(sci, 5);

	SSM (sci, SCI_SETLEXER, SCLEX_MAKEFILE, 0);

	SSM(sci, SCI_STYLESETFORE, SCE_MAKE_DEFAULT, types[GEANY_FILETYPES_MAKE]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_MAKE_DEFAULT, types[GEANY_FILETYPES_MAKE]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_MAKE_DEFAULT, types[GEANY_FILETYPES_MAKE]->styling[0][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_MAKE_COMMENT, types[GEANY_FILETYPES_MAKE]->styling[1][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_MAKE_COMMENT, types[GEANY_FILETYPES_MAKE]->styling[1][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_MAKE_COMMENT, types[GEANY_FILETYPES_MAKE]->styling[1][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_MAKE_PREPROCESSOR, types[GEANY_FILETYPES_MAKE]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_MAKE_PREPROCESSOR, types[GEANY_FILETYPES_MAKE]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_MAKE_PREPROCESSOR, types[GEANY_FILETYPES_MAKE]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_MAKE_IDENTIFIER, types[GEANY_FILETYPES_MAKE]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_MAKE_IDENTIFIER, types[GEANY_FILETYPES_MAKE]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_MAKE_IDENTIFIER, types[GEANY_FILETYPES_MAKE]->styling[3][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_MAKE_OPERATOR, types[GEANY_FILETYPES_MAKE]->styling[4][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_MAKE_OPERATOR, types[GEANY_FILETYPES_MAKE]->styling[4][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_MAKE_OPERATOR, types[GEANY_FILETYPES_MAKE]->styling[4][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_MAKE_TARGET, types[GEANY_FILETYPES_MAKE]->styling[5][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_MAKE_TARGET, types[GEANY_FILETYPES_MAKE]->styling[5][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_MAKE_TARGET, types[GEANY_FILETYPES_MAKE]->styling[5][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_MAKE_IDEOL, types[GEANY_FILETYPES_MAKE]->styling[6][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_MAKE_IDEOL, types[GEANY_FILETYPES_MAKE]->styling[6][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_MAKE_IDEOL, types[GEANY_FILETYPES_MAKE]->styling[6][2]);
}


void styleset_tex_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.tex", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_TEX] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "default", "0xFF0000", "0xffffff", "false", types[GEANY_FILETYPES_TEX]->styling[0]);
	styleset_get_hex(config, "styling", "command", "0x0000ff", "0xffffff", "true", types[GEANY_FILETYPES_TEX]->styling[1]);
	styleset_get_hex(config, "styling", "tag", "0x7F7F00", "0xffffff", "true", types[GEANY_FILETYPES_TEX]->styling[2]);
	styleset_get_hex(config, "styling", "math", "0x007F00", "0xffffff", "false", types[GEANY_FILETYPES_TEX]->styling[3]);
	styleset_get_hex(config, "styling", "comment", "0x007F00", "0xffffff", "false", types[GEANY_FILETYPES_TEX]->styling[4]);

	types[GEANY_FILETYPES_TEX]->keywords = g_new(gchar*, 2);
	types[GEANY_FILETYPES_TEX]->keywords[0] = styleset_get_string(config, "keywords", "primary");
	if (types[GEANY_FILETYPES_TEX]->keywords[0] == NULL)
			types[GEANY_FILETYPES_TEX]->keywords[0] = g_strdup("section subsection begin item");
	types[GEANY_FILETYPES_TEX]->keywords[1] = NULL;

	g_key_file_free(config);
}


void styleset_tex(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_TEX] == NULL) styleset_tex_init();

	styleset_common(sci, 5);

	SSM (sci, SCI_SETLEXER, SCLEX_LATEX, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_TEX]->keywords[0]);

	SSM(sci, SCI_STYLESETFORE, SCE_L_DEFAULT, types[GEANY_FILETYPES_TEX]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_L_DEFAULT, types[GEANY_FILETYPES_TEX]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_L_DEFAULT, types[GEANY_FILETYPES_TEX]->styling[0][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_L_COMMAND, types[GEANY_FILETYPES_TEX]->styling[1][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_L_COMMAND, types[GEANY_FILETYPES_TEX]->styling[1][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_L_COMMAND, types[GEANY_FILETYPES_TEX]->styling[1][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_L_TAG, types[GEANY_FILETYPES_TEX]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_L_TAG, types[GEANY_FILETYPES_TEX]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_L_TAG, types[GEANY_FILETYPES_TEX]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_L_MATH, types[GEANY_FILETYPES_TEX]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_L_MATH, types[GEANY_FILETYPES_TEX]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_L_MATH, types[GEANY_FILETYPES_TEX]->styling[3][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_L_COMMENT, types[GEANY_FILETYPES_TEX]->styling[4][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_L_COMMENT, types[GEANY_FILETYPES_TEX]->styling[4][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_L_COMMENT, types[GEANY_FILETYPES_TEX]->styling[4][2]);
	SSM(sci, SCI_STYLESETITALIC, SCE_L_COMMENT, TRUE);
}


void styleset_php(ScintillaObject *sci)
{
	styleset_common(sci, 7);

	// enable folding for retrieval of current function (utils_get_current_tag)
	SSM (sci, SCI_SETPROPERTY, (sptr_t) "fold", (sptr_t) "1");

	SSM (sci, SCI_SETPROPERTY, (sptr_t) "phpscript.mode", (sptr_t) "1");
	SSM (sci, SCI_SETLEXER, SCLEX_HTML, 0);

	// DWELL notification for URL highlighting
	SSM(sci, SCI_SETMOUSEDWELLTIME, 500, 0);

	// use the same colouring for HTML; XML and so on
	styleset_markup(sci);

}


void styleset_markup_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.markup", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_XML] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "html_default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[0]);
	styleset_get_hex(config, "styling", "html_tag", "0x990000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[1]);
	styleset_get_hex(config, "styling", "html_tagunknown", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[2]);
	styleset_get_hex(config, "styling", "html_attribute", "0x007F00", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[3]);
	styleset_get_hex(config, "styling", "html_attributeunknown", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[4]);
	styleset_get_hex(config, "styling", "html_number", "0x800080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[5]);
	styleset_get_hex(config, "styling", "html_doublestring", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[6]);
	styleset_get_hex(config, "styling", "html_singlestring", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[7]);
	styleset_get_hex(config, "styling", "html_other", "0x800080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[8]);
	styleset_get_hex(config, "styling", "html_comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[9]);
	styleset_get_hex(config, "styling", "html_entity", "0x800080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[10]);
	styleset_get_hex(config, "styling", "html_tagend", "0x800000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[11]);
	styleset_get_hex(config, "styling", "html_xmlstart", "0x990000", "0xf0f0f0", "false", types[GEANY_FILETYPES_XML]->styling[12]);
	styleset_get_hex(config, "styling", "html_xmlend", "0x990000", "0xf0f0f0", "false", types[GEANY_FILETYPES_XML]->styling[13]);
	styleset_get_hex(config, "styling", "html_script", "0x800000", "0xf0f0f0", "false", types[GEANY_FILETYPES_XML]->styling[14]);
	styleset_get_hex(config, "styling", "html_asp", "0x4f4f00", "0xf0f0f0", "false", types[GEANY_FILETYPES_XML]->styling[15]);
	styleset_get_hex(config, "styling", "html_aspat", "0x4f4f00", "0xf0f0f0", "false", types[GEANY_FILETYPES_XML]->styling[16]);
	styleset_get_hex(config, "styling", "html_cdata", "0x990066", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[17]);
	styleset_get_hex(config, "styling", "html_question", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[18]);
	styleset_get_hex(config, "styling", "html_value", "0x990066", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[19]);
	styleset_get_hex(config, "styling", "html_xccomment", "0x990066", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[20]);

	styleset_get_hex(config, "styling", "sgml_default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[21]);
	styleset_get_hex(config, "styling", "sgml_comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[22]);
	styleset_get_hex(config, "styling", "sgml_special", "0x007F00", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[23]);
	styleset_get_hex(config, "styling", "sgml_command", "0x991111", "0xffffff", "true", types[GEANY_FILETYPES_XML]->styling[24]);
	styleset_get_hex(config, "styling", "sgml_doublestring", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[25]);
	styleset_get_hex(config, "styling", "sgml_simplestring", "0x004040", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[26]);
	styleset_get_hex(config, "styling", "sgml_1st_param", "0x804040", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[27]);
	styleset_get_hex(config, "styling", "sgml_entity", "0x101030", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[28]);
	styleset_get_hex(config, "styling", "sgml_block_default", "0x100000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[29]);
	styleset_get_hex(config, "styling", "sgml_1st_param_comment", "0x906040", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[30]);
	styleset_get_hex(config, "styling", "sgml_error", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[31]);

	styleset_get_hex(config, "styling", "php_default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[32]);
	styleset_get_hex(config, "styling", "php_simplestring", "0x008000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[33]);
	styleset_get_hex(config, "styling", "php_hstring", "0x008000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[34]);
	styleset_get_hex(config, "styling", "php_number", "0x006060", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[35]);
	styleset_get_hex(config, "styling", "php_word", "0x990000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[36]);
	styleset_get_hex(config, "styling", "php_variable", "0x00007F", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[37]);
	styleset_get_hex(config, "styling", "php_comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[38]);
	styleset_get_hex(config, "styling", "php_commentline", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[39]);
	styleset_get_hex(config, "styling", "php_operator", "0x602010", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[40]);
	styleset_get_hex(config, "styling", "php_hstring_variable", "0x601010", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[41]);
	styleset_get_hex(config, "styling", "php_complex_variable", "0x105010", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[42]);

	styleset_get_hex(config, "styling", "jscript_start", "0x808000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[43]);
	styleset_get_hex(config, "styling", "jscript_default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[44]);
	styleset_get_hex(config, "styling", "jscript_comment", "0x222222", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[45]);
	styleset_get_hex(config, "styling", "jscript_commentline", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[46]);
	styleset_get_hex(config, "styling", "jscript_commentdoc", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[47]);
	styleset_get_hex(config, "styling", "jscript_number", "0x606000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[48]);
	styleset_get_hex(config, "styling", "jscript_word", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[49]);
	styleset_get_hex(config, "styling", "jscript_keyword", "0x001050", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[50]);
	styleset_get_hex(config, "styling", "jscript_doublestring", "0x080080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[51]);
	styleset_get_hex(config, "styling", "jscript_singlestring", "0x080080", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[52]);
	styleset_get_hex(config, "styling", "jscript_symbols", "0x501000", "0xffffff", "false", types[GEANY_FILETYPES_XML]->styling[53]);
	styleset_get_hex(config, "styling", "jscript_stringeol", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_XML]->styling[54]);

	types[GEANY_FILETYPES_XML]->keywords = g_new(gchar*, 7);
	types[GEANY_FILETYPES_XML]->keywords[0] = styleset_get_string(config, "keywords", "html");
	if (types[GEANY_FILETYPES_XML]->keywords[0] == NULL)
			types[GEANY_FILETYPES_XML]->keywords[0] = g_strdup("a abbr acronym address applet area b base basefont bdo big blockquote body br button caption center cite code col colgroup dd del dfn dir div dl dt em embed fieldset font form frame frameset h1 h2 h3 h4 h5 h6 head hr html i iframe img input ins isindex kbd label legend li link map menu meta noframes noscript object ol optgroup option p param pre q quality s samp script select small span strike strong style sub sup table tbody td textarea tfoot th thead title tr tt u ul var xmlns leftmargin topmargin abbr accept-charset accept accesskey action align alink alt archive axis background bgcolor border cellpadding cellspacing char charoff charset checked cite class classid clear codebase codetype color cols colspan compact content coords data datafld dataformatas datapagesize datasrc datetime declare defer dir disabled enctype face for frame frameborder selected headers height href hreflang hspace http-equiv id ismap label lang language link longdesc marginwidth marginheight maxlength media framespacing method multiple name nohref noresize noshade nowrap object onblur onchange onclick ondblclick onfocus onkeydown onkeypress onkeyup onload onmousedown onmousemove onmouseover onmouseout onmouseup onreset onselect onsubmit onunload profile prompt pluginspage readonly rel rev rows rowspan rules scheme scope scrolling shape size span src standby start style summary tabindex target text title type usemap valign value valuetype version vlink vspace width text password checkbox radio submit reset file hidden image public doctype xml");
	types[GEANY_FILETYPES_XML]->keywords[1] = styleset_get_string(config, "keywords", "javascript");
	if (types[GEANY_FILETYPES_XML]->keywords[1] == NULL)
			types[GEANY_FILETYPES_XML]->keywords[1] = g_strdup("break this for while null else var false void new delete typeof if in continue true function with return case super extends do const try debugger catch switch finally enum export default class throw import length concat join pop push reverse shift slice splice sort unshift Date Infinity NaN undefined escape eval isFinite isNaN Number parseFloat parseInt string unescape Math abs acos asin atan atan2 ceil cos exp floor log max min pow random round sin sqrt tan MAX_VALUE MIN_VALUE NEGATIVE_INFINITY POSITIVE_INFINITY toString valueOf String length anchor big bold charAt charCodeAt concat fixed fontcolor fontsize fromCharCode indexOf italics lastIndexOf link slice small split strike sub substr substring sup toLowerCase toUpperCase");
	types[GEANY_FILETYPES_XML]->keywords[2] = styleset_get_string(config, "keywords", "vbscript");
	if (types[GEANY_FILETYPES_XML]->keywords[2] == NULL)
			types[GEANY_FILETYPES_XML]->keywords[2] = g_strdup("and as byref byval case call const continue dim do each else elseif end error exit false for function global goto if in loop me new next not nothing on optional or private public redim rem resume select set sub then to true type while with boolean byte currency date double integer long object single string type variant");
	types[GEANY_FILETYPES_XML]->keywords[3] = styleset_get_string(config, "keywords", "python");
	if (types[GEANY_FILETYPES_XML]->keywords[3] == NULL)
			types[GEANY_FILETYPES_XML]->keywords[3] = g_strdup("and assert break class continue complex def del elif else except exec finally for from global if import in inherit is int lambda not or pass print raise return tuple try unicode while yield long float str list");
	types[GEANY_FILETYPES_XML]->keywords[4] = styleset_get_string(config, "keywords", "php");
	if (types[GEANY_FILETYPES_XML]->keywords[4] == NULL)
			types[GEANY_FILETYPES_XML]->keywords[4] = g_strdup("and or xor FILE exception php_user_filter LINE array as break case cfunction class const continue declare default die do echo else elseif empty enddeclare endfor endforeach endif endswitch endwhile eval exit extends for foreach function global if include include_once isset list new old_function print require require_once return static switch unset use var while FUNCTION CLASS	METHOD");
	types[GEANY_FILETYPES_XML]->keywords[5] = styleset_get_string(config, "keywords", "sgml");
	if (types[GEANY_FILETYPES_XML]->keywords[5] == NULL)
			types[GEANY_FILETYPES_XML]->keywords[5] = g_strdup("ELEMENT DOCTYPE ATTLIST ENTITY NOTATION");
	types[GEANY_FILETYPES_XML]->keywords[6] = NULL;

	g_key_file_free(config);
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

	SSM(sci, SCI_SETWHITESPACEFORE, 1, 0xc0c0c0);

	// hotspotting, nice thing
	SSM(sci, SCI_SETHOTSPOTACTIVEFORE, 1, 0xff0000);
	SSM(sci, SCI_SETHOTSPOTACTIVEUNDERLINE, 1, 0);
	SSM(sci, SCI_SETHOTSPOTSINGLELINE, 1, 0);
	SSM(sci, SCI_STYLESETHOTSPOT, SCE_H_QUESTION, 1);


	SSM(sci, SCI_STYLESETFORE, SCE_H_DEFAULT, types[GEANY_FILETYPES_XML]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_DEFAULT, types[GEANY_FILETYPES_XML]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_DEFAULT, types[GEANY_FILETYPES_XML]->styling[0][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_TAG, types[GEANY_FILETYPES_XML]->styling[1][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_TAG, types[GEANY_FILETYPES_XML]->styling[1][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_TAG, types[GEANY_FILETYPES_XML]->styling[1][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_TAGUNKNOWN, types[GEANY_FILETYPES_XML]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_TAGUNKNOWN, types[GEANY_FILETYPES_XML]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_TAGUNKNOWN, types[GEANY_FILETYPES_XML]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_ATTRIBUTE, types[GEANY_FILETYPES_XML]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_ATTRIBUTE, types[GEANY_FILETYPES_XML]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_ATTRIBUTE, types[GEANY_FILETYPES_XML]->styling[3][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_ATTRIBUTEUNKNOWN, types[GEANY_FILETYPES_XML]->styling[4][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_ATTRIBUTEUNKNOWN, types[GEANY_FILETYPES_XML]->styling[4][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_ATTRIBUTEUNKNOWN, types[GEANY_FILETYPES_XML]->styling[4][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_NUMBER, types[GEANY_FILETYPES_XML]->styling[5][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_NUMBER, types[GEANY_FILETYPES_XML]->styling[5][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_NUMBER, types[GEANY_FILETYPES_XML]->styling[5][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_DOUBLESTRING, types[GEANY_FILETYPES_XML]->styling[6][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_DOUBLESTRING, types[GEANY_FILETYPES_XML]->styling[6][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_DOUBLESTRING, types[GEANY_FILETYPES_XML]->styling[6][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SINGLESTRING, types[GEANY_FILETYPES_XML]->styling[7][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SINGLESTRING, types[GEANY_FILETYPES_XML]->styling[7][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SINGLESTRING, types[GEANY_FILETYPES_XML]->styling[7][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_OTHER, types[GEANY_FILETYPES_XML]->styling[8][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_OTHER, types[GEANY_FILETYPES_XML]->styling[8][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_OTHER, types[GEANY_FILETYPES_XML]->styling[8][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_COMMENT, types[GEANY_FILETYPES_XML]->styling[9][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_COMMENT, types[GEANY_FILETYPES_XML]->styling[9][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_COMMENT, types[GEANY_FILETYPES_XML]->styling[9][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_ENTITY, types[GEANY_FILETYPES_XML]->styling[10][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_ENTITY, types[GEANY_FILETYPES_XML]->styling[10][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_ENTITY, types[GEANY_FILETYPES_XML]->styling[10][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_TAGEND, types[GEANY_FILETYPES_XML]->styling[11][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_TAGEND, types[GEANY_FILETYPES_XML]->styling[11][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_TAGEND, types[GEANY_FILETYPES_XML]->styling[11][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_XMLSTART, types[GEANY_FILETYPES_XML]->styling[12][0]);// <?
	SSM(sci, SCI_STYLESETBACK, SCE_H_XMLSTART, types[GEANY_FILETYPES_XML]->styling[12][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_XMLSTART, types[GEANY_FILETYPES_XML]->styling[12][2]);
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_H_XMLSTART, 1);

	SSM(sci, SCI_STYLESETFORE, SCE_H_XMLEND, types[GEANY_FILETYPES_XML]->styling[13][0]);// ?>
	SSM(sci, SCI_STYLESETBACK, SCE_H_XMLEND, types[GEANY_FILETYPES_XML]->styling[13][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_XMLEND, types[GEANY_FILETYPES_XML]->styling[13][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SCRIPT, types[GEANY_FILETYPES_XML]->styling[14][0]);// <script
	SSM(sci, SCI_STYLESETBACK, SCE_H_SCRIPT, types[GEANY_FILETYPES_XML]->styling[14][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SCRIPT, types[GEANY_FILETYPES_XML]->styling[14][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_ASP, types[GEANY_FILETYPES_XML]->styling[15][0]);	// <% ... %>
	SSM(sci, SCI_STYLESETBACK, SCE_H_ASP, types[GEANY_FILETYPES_XML]->styling[15][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_ASP, types[GEANY_FILETYPES_XML]->styling[15][2]);
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_H_ASP, 1);

	SSM(sci, SCI_STYLESETFORE, SCE_H_ASPAT, types[GEANY_FILETYPES_XML]->styling[16][0]);	// <%@ ... %>
	SSM(sci, SCI_STYLESETBACK, SCE_H_ASPAT, types[GEANY_FILETYPES_XML]->styling[16][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_ASPAT, types[GEANY_FILETYPES_XML]->styling[16][2]);
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_H_ASPAT, 1);

	SSM(sci, SCI_STYLESETFORE, SCE_H_CDATA, types[GEANY_FILETYPES_XML]->styling[17][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_CDATA, types[GEANY_FILETYPES_XML]->styling[17][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_CDATA, types[GEANY_FILETYPES_XML]->styling[17][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_QUESTION, types[GEANY_FILETYPES_XML]->styling[18][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_QUESTION, types[GEANY_FILETYPES_XML]->styling[18][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_QUESTION, types[GEANY_FILETYPES_XML]->styling[18][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_VALUE, types[GEANY_FILETYPES_XML]->styling[19][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_VALUE, types[GEANY_FILETYPES_XML]->styling[19][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_VALUE, types[GEANY_FILETYPES_XML]->styling[19][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_XCCOMMENT, types[GEANY_FILETYPES_XML]->styling[20][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_XCCOMMENT, types[GEANY_FILETYPES_XML]->styling[20][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_XCCOMMENT, types[GEANY_FILETYPES_XML]->styling[20][2]);


	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_DEFAULT, types[GEANY_FILETYPES_XML]->styling[21][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_DEFAULT, types[GEANY_FILETYPES_XML]->styling[21][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_DEFAULT, types[GEANY_FILETYPES_XML]->styling[21][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_COMMENT, types[GEANY_FILETYPES_XML]->styling[22][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_COMMENT, types[GEANY_FILETYPES_XML]->styling[22][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_COMMENT, types[GEANY_FILETYPES_XML]->styling[22][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_SPECIAL, types[GEANY_FILETYPES_XML]->styling[23][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_SPECIAL, types[GEANY_FILETYPES_XML]->styling[23][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_SPECIAL, types[GEANY_FILETYPES_XML]->styling[23][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_COMMAND, types[GEANY_FILETYPES_XML]->styling[24][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_COMMAND, types[GEANY_FILETYPES_XML]->styling[24][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_COMMAND, types[GEANY_FILETYPES_XML]->styling[24][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_DOUBLESTRING, types[GEANY_FILETYPES_XML]->styling[25][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_DOUBLESTRING, types[GEANY_FILETYPES_XML]->styling[25][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_DOUBLESTRING, types[GEANY_FILETYPES_XML]->styling[25][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_SIMPLESTRING, types[GEANY_FILETYPES_XML]->styling[26][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_SIMPLESTRING, types[GEANY_FILETYPES_XML]->styling[26][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_SIMPLESTRING, types[GEANY_FILETYPES_XML]->styling[26][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_1ST_PARAM, types[GEANY_FILETYPES_XML]->styling[27][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_1ST_PARAM, types[GEANY_FILETYPES_XML]->styling[27][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_1ST_PARAM, types[GEANY_FILETYPES_XML]->styling[27][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_ENTITY, types[GEANY_FILETYPES_XML]->styling[28][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_ENTITY, types[GEANY_FILETYPES_XML]->styling[28][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_ENTITY, types[GEANY_FILETYPES_XML]->styling[28][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_BLOCK_DEFAULT, types[GEANY_FILETYPES_XML]->styling[29][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_BLOCK_DEFAULT, types[GEANY_FILETYPES_XML]->styling[29][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_BLOCK_DEFAULT, types[GEANY_FILETYPES_XML]->styling[29][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_1ST_PARAM_COMMENT, types[GEANY_FILETYPES_XML]->styling[30][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_1ST_PARAM_COMMENT, types[GEANY_FILETYPES_XML]->styling[30][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_1ST_PARAM_COMMENT, types[GEANY_FILETYPES_XML]->styling[30][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_ERROR, types[GEANY_FILETYPES_XML]->styling[31][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_ERROR, types[GEANY_FILETYPES_XML]->styling[31][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_ERROR, types[GEANY_FILETYPES_XML]->styling[31][2]);


	SSM(sci, SCI_STYLESETFORE, SCE_HB_DEFAULT, 0x000000);
	SSM(sci, SCI_STYLESETBACK, SCE_HB_DEFAULT, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HB_COMMENTLINE, 0x808080);
	SSM(sci, SCI_STYLESETBACK, SCE_HB_COMMENTLINE, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HB_NUMBER, 0x008080);
	SSM(sci, SCI_STYLESETBACK, SCE_HB_NUMBER, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HB_WORD, 0x008080);
	SSM(sci, SCI_STYLESETBACK, SCE_HB_WORD, 0xffffff);
	SSM(sci, SCI_STYLESETBOLD, SCE_HB_WORD, 1);

	SSM(sci, SCI_STYLESETFORE, SCE_HB_STRING, 0x008000);
	SSM(sci, SCI_STYLESETBACK, SCE_HB_STRING, 0x008000);

	SSM(sci, SCI_STYLESETFORE, SCE_HB_IDENTIFIER, 0x103000);
	SSM(sci, SCI_STYLESETBACK, SCE_HB_IDENTIFIER, 0xffffff);
//~ #define SCE_HB_START 70

	// Show the whole section of VBScript
/*	for (bstyle=SCE_HB_DEFAULT; bstyle<=SCE_HB_STRINGEOL; bstyle++) {
		SSM(sci, SCI_STYLESETBACK, bstyle, 0xf5f5f5);
		// This call extends the backround colour of the last style on the line to the edge of the window
		SSM(sci, SCI_STYLESETEOLFILLED, bstyle, 1);
	}
*/
	SSM(sci,SCI_STYLESETBACK, SCE_HB_STRINGEOL, 0x7F7FFF);

	SSM(sci, SCI_STYLESETFORE, SCE_HBA_DEFAULT, 0x000000);
	SSM(sci, SCI_STYLESETBACK, SCE_HBA_DEFAULT, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HBA_COMMENTLINE, 0x808080);
	SSM(sci, SCI_STYLESETBACK, SCE_HBA_COMMENTLINE, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HBA_NUMBER, 0x008080);
	SSM(sci, SCI_STYLESETBACK, SCE_HBA_NUMBER, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HBA_WORD, 0x008080);
	SSM(sci, SCI_STYLESETBACK, SCE_HBA_WORD, 0xffffff);
	SSM(sci, SCI_STYLESETBOLD, SCE_HBA_WORD, 1);

	SSM(sci, SCI_STYLESETFORE, SCE_HBA_STRING, 0x008000);
	SSM(sci, SCI_STYLESETBACK, SCE_HBA_STRING, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HBA_IDENTIFIER, 0x103000);
	SSM(sci, SCI_STYLESETBACK, SCE_HBA_IDENTIFIER, 0xffffff);


	SSM(sci, SCI_STYLESETFORE, SCE_HJ_START, types[GEANY_FILETYPES_XML]->styling[43][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HJ_START, types[GEANY_FILETYPES_XML]->styling[43][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HJ_START, types[GEANY_FILETYPES_XML]->styling[43][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HJ_DEFAULT, types[GEANY_FILETYPES_XML]->styling[44][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HJ_DEFAULT, types[GEANY_FILETYPES_XML]->styling[44][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HJ_DEFAULT, types[GEANY_FILETYPES_XML]->styling[44][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HJ_COMMENT, types[GEANY_FILETYPES_XML]->styling[45][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HJ_COMMENT, types[GEANY_FILETYPES_XML]->styling[45][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HJ_COMMENT, types[GEANY_FILETYPES_XML]->styling[45][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HJ_COMMENTLINE, types[GEANY_FILETYPES_XML]->styling[46][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HJ_COMMENTLINE, types[GEANY_FILETYPES_XML]->styling[46][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HJ_COMMENTLINE, types[GEANY_FILETYPES_XML]->styling[46][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HJ_COMMENTDOC, types[GEANY_FILETYPES_XML]->styling[47][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HJ_COMMENTDOC, types[GEANY_FILETYPES_XML]->styling[47][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HJ_COMMENTDOC, types[GEANY_FILETYPES_XML]->styling[47][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HJ_NUMBER, types[GEANY_FILETYPES_XML]->styling[48][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HJ_NUMBER, types[GEANY_FILETYPES_XML]->styling[48][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HJ_NUMBER, types[GEANY_FILETYPES_XML]->styling[48][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HJ_WORD, types[GEANY_FILETYPES_XML]->styling[49][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HJ_WORD, types[GEANY_FILETYPES_XML]->styling[49][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HJ_WORD, types[GEANY_FILETYPES_XML]->styling[49][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HJ_KEYWORD, types[GEANY_FILETYPES_XML]->styling[50][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HJ_KEYWORD, types[GEANY_FILETYPES_XML]->styling[50][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HJ_KEYWORD, types[GEANY_FILETYPES_XML]->styling[50][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HJ_DOUBLESTRING, types[GEANY_FILETYPES_XML]->styling[51][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HJ_DOUBLESTRING, types[GEANY_FILETYPES_XML]->styling[51][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HJ_DOUBLESTRING, types[GEANY_FILETYPES_XML]->styling[51][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HJ_SINGLESTRING, types[GEANY_FILETYPES_XML]->styling[52][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HJ_SINGLESTRING, types[GEANY_FILETYPES_XML]->styling[52][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HJ_SINGLESTRING, types[GEANY_FILETYPES_XML]->styling[52][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HJ_SYMBOLS, types[GEANY_FILETYPES_XML]->styling[53][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HJ_SYMBOLS, types[GEANY_FILETYPES_XML]->styling[53][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HJ_SYMBOLS, types[GEANY_FILETYPES_XML]->styling[53][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HJ_STRINGEOL, types[GEANY_FILETYPES_XML]->styling[54][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HJ_STRINGEOL, types[GEANY_FILETYPES_XML]->styling[54][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HJ_STRINGEOL, types[GEANY_FILETYPES_XML]->styling[54][2]);


	SSM(sci, SCI_STYLESETFORE, SCE_HP_START, 0x808000);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_START, 0xf0f0f0);
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_HP_START, 1);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_DEFAULT, 0x000000);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_DEFAULT, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_COMMENTLINE, 0x808080);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_COMMENTLINE, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_NUMBER, 0x008080);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_NUMBER, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_WORD, 0x990000);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_WORD, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_STRING, 0x008000);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_STRING, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_CHARACTER, 0x006060);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_CHARACTER, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_TRIPLE, 0x002060);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_TRIPLE, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_TRIPLEDOUBLE, 0x002060);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_TRIPLEDOUBLE, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_CLASSNAME, 0x202010);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_CLASSNAME, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_CLASSNAME, 0x102020);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_CLASSNAME, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_OPERATOR, 0x602020);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_OPERATOR, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_IDENTIFIER, 0x001060);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_IDENTIFIER, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_START, 0x808000);
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_START, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_DEFAULT, 0x000000);
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_DEFAULT, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_COMMENTLINE, 0x808080);
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_COMMENTLINE, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_NUMBER, 0x408000);
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_NUMBER, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_STRING, 0x008080);
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_STRING, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_CHARACTER, 0x505080);
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_CHARACTER, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_WORD, 0x990000);
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_WORD, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_TRIPLE, 0x002060);
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_TRIPLE, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_TRIPLEDOUBLE, 0x002060);
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_TRIPLEDOUBLE, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_CLASSNAME, 0x202010);
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_CLASSNAME, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_DEFNAME, 0x102020);
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_DEFNAME, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_OPERATOR, 0x601010);
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_OPERATOR, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPA_IDENTIFIER, 0x105010);
	SSM(sci, SCI_STYLESETBACK, SCE_HPA_IDENTIFIER, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_COMMENTLINE, 0x808080);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_COMMENTLINE, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HP_NUMBER, 0x408000);
	SSM(sci, SCI_STYLESETBACK, SCE_HP_NUMBER, 0xffffff);

	SSM(sci, SCI_STYLESETFORE, SCE_HPHP_DEFAULT, types[GEANY_FILETYPES_XML]->styling[32][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HPHP_DEFAULT, types[GEANY_FILETYPES_XML]->styling[32][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HPHP_DEFAULT, types[GEANY_FILETYPES_XML]->styling[32][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HPHP_SIMPLESTRING, types[GEANY_FILETYPES_XML]->styling[33][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HPHP_SIMPLESTRING, types[GEANY_FILETYPES_XML]->styling[33][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HPHP_SIMPLESTRING, types[GEANY_FILETYPES_XML]->styling[33][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HPHP_HSTRING, types[GEANY_FILETYPES_XML]->styling[34][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HPHP_HSTRING, types[GEANY_FILETYPES_XML]->styling[34][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HPHP_HSTRING, types[GEANY_FILETYPES_XML]->styling[34][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HPHP_NUMBER, types[GEANY_FILETYPES_XML]->styling[35][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HPHP_NUMBER, types[GEANY_FILETYPES_XML]->styling[35][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HPHP_NUMBER, types[GEANY_FILETYPES_XML]->styling[35][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HPHP_WORD, types[GEANY_FILETYPES_XML]->styling[36][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HPHP_WORD, types[GEANY_FILETYPES_XML]->styling[36][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HPHP_WORD, types[GEANY_FILETYPES_XML]->styling[36][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HPHP_VARIABLE, types[GEANY_FILETYPES_XML]->styling[37][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HPHP_VARIABLE, types[GEANY_FILETYPES_XML]->styling[37][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HPHP_VARIABLE, types[GEANY_FILETYPES_XML]->styling[37][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HPHP_COMMENT, types[GEANY_FILETYPES_XML]->styling[38][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HPHP_COMMENT, types[GEANY_FILETYPES_XML]->styling[38][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HPHP_COMMENT, types[GEANY_FILETYPES_XML]->styling[38][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HPHP_COMMENTLINE, types[GEANY_FILETYPES_XML]->styling[39][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HPHP_COMMENTLINE, types[GEANY_FILETYPES_XML]->styling[39][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HPHP_COMMENTLINE, types[GEANY_FILETYPES_XML]->styling[39][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HPHP_OPERATOR, types[GEANY_FILETYPES_XML]->styling[40][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HPHP_OPERATOR, types[GEANY_FILETYPES_XML]->styling[40][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HPHP_OPERATOR, types[GEANY_FILETYPES_XML]->styling[40][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HPHP_HSTRING_VARIABLE, types[GEANY_FILETYPES_XML]->styling[41][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HPHP_HSTRING_VARIABLE, types[GEANY_FILETYPES_XML]->styling[41][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HPHP_HSTRING_VARIABLE, types[GEANY_FILETYPES_XML]->styling[41][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_HPHP_COMPLEX_VARIABLE, types[GEANY_FILETYPES_XML]->styling[42][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_HPHP_COMPLEX_VARIABLE, types[GEANY_FILETYPES_XML]->styling[42][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_HPHP_COMPLEX_VARIABLE, types[GEANY_FILETYPES_XML]->styling[42][2]);
}


void styleset_java_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.java", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_JAVA] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[0]);
	styleset_get_hex(config, "styling", "comment", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[1]);
	styleset_get_hex(config, "styling", "commentline", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[2]);
	styleset_get_hex(config, "styling", "commentdoc", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[3]);
	styleset_get_hex(config, "styling", "number", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[4]);
	styleset_get_hex(config, "styling", "word", "0x991111", "0xffffff", "true", types[GEANY_FILETYPES_JAVA]->styling[5]);
	styleset_get_hex(config, "styling", "word2", "0x00007F", "0xffffff", "true", types[GEANY_FILETYPES_JAVA]->styling[6]);
	styleset_get_hex(config, "styling", "string", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[7]);
	styleset_get_hex(config, "styling", "character", "0x004040", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[8]);
	styleset_get_hex(config, "styling", "uuid", "0x804040", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[9]);
	styleset_get_hex(config, "styling", "preprocessor", "0x7F7F00", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[10]);
	styleset_get_hex(config, "styling", "operator", "0x101030", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[11]);
	styleset_get_hex(config, "styling", "identifier", "0x100000", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[12]);
	styleset_get_hex(config, "styling", "stringeol", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_JAVA]->styling[13]);
	styleset_get_hex(config, "styling", "verbatim", "0x906040", "0x0000ff", "false", types[GEANY_FILETYPES_JAVA]->styling[14]);
	styleset_get_hex(config, "styling", "regex", "0x905010", "0xffffff", "false", types[GEANY_FILETYPES_JAVA]->styling[15]);
	styleset_get_hex(config, "styling", "commentlinedoc", "0x0000ff", "0xffffff", "true", types[GEANY_FILETYPES_JAVA]->styling[16]);
	styleset_get_hex(config, "styling", "commentdockeyword", "0x0000ff", "0xffffff", "true", types[GEANY_FILETYPES_JAVA]->styling[17]);
	styleset_get_hex(config, "styling", "globalclass", "0x109040", "0xffffff", "true", types[GEANY_FILETYPES_JAVA]->styling[18]);

	types[GEANY_FILETYPES_JAVA]->keywords = g_new(gchar*, 5);
	types[GEANY_FILETYPES_JAVA]->keywords[0] = styleset_get_string(config, "keywords", "primary");
	if (types[GEANY_FILETYPES_JAVA]->keywords[0] == NULL)
			types[GEANY_FILETYPES_JAVA]->keywords[0] = g_strdup("	abstract assert break case catch class \
										const continue default do else extends final finally for future \
										generic goto if implements import inner instanceof interface \
										native new outer package private protected public rest \
										return static super switch synchronized this throw throws \
										transient try var volatile while");
	types[GEANY_FILETYPES_JAVA]->keywords[1] = styleset_get_string(config, "keywords", "secondary");
	if (types[GEANY_FILETYPES_JAVA]->keywords[1] == NULL)
			types[GEANY_FILETYPES_JAVA]->keywords[1] = g_strdup("boolean byte char double float int long null short void NULL");
	types[GEANY_FILETYPES_JAVA]->keywords[2] = styleset_get_string(config, "keywords", "doccomment");
	if (types[GEANY_FILETYPES_JAVA]->keywords[2] == NULL)
			types[GEANY_FILETYPES_JAVA]->keywords[2] = g_strdup("return param author");
	types[GEANY_FILETYPES_JAVA]->keywords[3] = styleset_get_string(config, "keywords", "typedefs");
	if (types[GEANY_FILETYPES_JAVA]->keywords[3] == NULL)
			types[GEANY_FILETYPES_JAVA]->keywords[3] = g_strdup("");
	types[GEANY_FILETYPES_JAVA]->keywords[4] = NULL;

	g_key_file_free(config);
}


void styleset_java(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_JAVA] == NULL) styleset_java_init();

	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_CPP, 0);

	// enable folding for retrieval of current function (utils_get_current_tag)
	SSM (sci, SCI_SETPROPERTY, (sptr_t) "fold", (sptr_t) "1");

	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) GEANY_WORDCHARS);
	SSM (sci, SCI_AUTOCSETMAXHEIGHT, 8, 0);

	SSM (sci, SCI_SETCONTROLCHARSYMBOL, 32, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_JAVA]->keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) types[GEANY_FILETYPES_JAVA]->keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) types[GEANY_FILETYPES_JAVA]->keywords[2]);
	SSM(sci, SCI_SETKEYWORDS, 4, (sptr_t) types[GEANY_FILETYPES_JAVA]->keywords[3]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_DEFAULT, types[GEANY_FILETYPES_JAVA]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_DEFAULT, types[GEANY_FILETYPES_JAVA]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_DEFAULT, types[GEANY_FILETYPES_JAVA]->styling[0][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENT, types[GEANY_FILETYPES_JAVA]->styling[1][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENT, types[GEANY_FILETYPES_JAVA]->styling[1][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_COMMENT, types[GEANY_FILETYPES_JAVA]->styling[1][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTLINE, types[GEANY_FILETYPES_JAVA]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTLINE, types[GEANY_FILETYPES_JAVA]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_COMMENTLINE, types[GEANY_FILETYPES_JAVA]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTDOC, types[GEANY_FILETYPES_JAVA]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTDOC, types[GEANY_FILETYPES_JAVA]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_COMMENTDOC, types[GEANY_FILETYPES_JAVA]->styling[3][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_NUMBER, types[GEANY_FILETYPES_JAVA]->styling[4][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_NUMBER, types[GEANY_FILETYPES_JAVA]->styling[4][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_DEFAULT, types[GEANY_FILETYPES_JAVA]->styling[4][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_WORD, types[GEANY_FILETYPES_JAVA]->styling[5][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_WORD, types[GEANY_FILETYPES_JAVA]->styling[5][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_WORD, types[GEANY_FILETYPES_JAVA]->styling[5][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_WORD2, types[GEANY_FILETYPES_JAVA]->styling[6][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_WORD2, types[GEANY_FILETYPES_JAVA]->styling[6][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_WORD2, types[GEANY_FILETYPES_JAVA]->styling[6][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_STRING, types[GEANY_FILETYPES_JAVA]->styling[7][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_STRING, types[GEANY_FILETYPES_JAVA]->styling[7][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_STRING, types[GEANY_FILETYPES_JAVA]->styling[7][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_CHARACTER, types[GEANY_FILETYPES_JAVA]->styling[8][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_CHARACTER, types[GEANY_FILETYPES_JAVA]->styling[8][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_CHARACTER, types[GEANY_FILETYPES_JAVA]->styling[8][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_UUID, types[GEANY_FILETYPES_JAVA]->styling[9][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_UUID, types[GEANY_FILETYPES_JAVA]->styling[9][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_UUID, types[GEANY_FILETYPES_JAVA]->styling[9][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_PREPROCESSOR, types[GEANY_FILETYPES_JAVA]->styling[10][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_PREPROCESSOR, types[GEANY_FILETYPES_JAVA]->styling[10][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_PREPROCESSOR, types[GEANY_FILETYPES_JAVA]->styling[10][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_OPERATOR, types[GEANY_FILETYPES_JAVA]->styling[11][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_OPERATOR, types[GEANY_FILETYPES_JAVA]->styling[11][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_OPERATOR, types[GEANY_FILETYPES_JAVA]->styling[11][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_IDENTIFIER, types[GEANY_FILETYPES_JAVA]->styling[12][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_IDENTIFIER, types[GEANY_FILETYPES_JAVA]->styling[12][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_IDENTIFIER, types[GEANY_FILETYPES_JAVA]->styling[12][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_STRINGEOL, types[GEANY_FILETYPES_JAVA]->styling[13][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_STRINGEOL, types[GEANY_FILETYPES_JAVA]->styling[13][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_STRINGEOL, types[GEANY_FILETYPES_JAVA]->styling[13][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_VERBATIM, types[GEANY_FILETYPES_JAVA]->styling[14][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_VERBATIM, types[GEANY_FILETYPES_JAVA]->styling[14][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_VERBATIM, types[GEANY_FILETYPES_JAVA]->styling[14][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_REGEX, types[GEANY_FILETYPES_JAVA]->styling[15][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_REGEX, types[GEANY_FILETYPES_JAVA]->styling[15][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_REGEX, types[GEANY_FILETYPES_JAVA]->styling[15][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTLINEDOC, types[GEANY_FILETYPES_JAVA]->styling[16][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTLINEDOC, types[GEANY_FILETYPES_JAVA]->styling[16][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_COMMENTLINEDOC, types[GEANY_FILETYPES_JAVA]->styling[16][2]);
	SSM(sci, SCI_STYLESETITALIC, SCE_C_COMMENTLINEDOC, TRUE);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTDOCKEYWORD, types[GEANY_FILETYPES_JAVA]->styling[17][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTDOCKEYWORD, types[GEANY_FILETYPES_JAVA]->styling[17][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_COMMENTDOCKEYWORD, types[GEANY_FILETYPES_JAVA]->styling[17][2]);
	SSM(sci, SCI_STYLESETITALIC, SCE_C_COMMENTDOCKEYWORD, TRUE);

	SSM(sci, SCI_STYLESETFORE, SCE_C_COMMENTDOCKEYWORDERROR, 0x0000ff);
	SSM(sci, SCI_STYLESETBACK, SCE_C_COMMENTDOCKEYWORDERROR, 0xFFFFFF);
	SSM(sci, SCI_STYLESETITALIC, SCE_C_COMMENTDOCKEYWORDERROR, TRUE);

	SSM(sci, SCI_STYLESETFORE, SCE_C_GLOBALCLASS, types[GEANY_FILETYPES_JAVA]->styling[18][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_C_GLOBALCLASS, types[GEANY_FILETYPES_JAVA]->styling[18][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_C_GLOBALCLASS, types[GEANY_FILETYPES_JAVA]->styling[18][2]);

	SSM(sci, SCI_SETWHITESPACEFORE, 1, 0xc0c0c0);
}


void styleset_perl_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.perl", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_PERL] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[0]);
	styleset_get_hex(config, "styling", "error", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[1]);
	styleset_get_hex(config, "styling", "commentline", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[2]);
	styleset_get_hex(config, "styling", "number", "0x007F00", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[3]);
	styleset_get_hex(config, "styling", "word", "0x991111", "0xffffff", "true", types[GEANY_FILETYPES_PERL]->styling[4]);
	styleset_get_hex(config, "styling", "string", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[5]);
	styleset_get_hex(config, "styling", "character", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[6]);
	styleset_get_hex(config, "styling", "preprocessor", "0x7F7F00", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[7]);
	styleset_get_hex(config, "styling", "operator", "0x101030", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[8]);
	styleset_get_hex(config, "styling", "identifier", "0x100000", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[9]);
	styleset_get_hex(config, "styling", "scalar", "0x00007F", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[10]);
	styleset_get_hex(config, "styling", "pod", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_PERL]->styling[11]);
	styleset_get_hex(config, "styling", "regex", "0x905010", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[12]);
	styleset_get_hex(config, "styling", "array", "0x905010", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[13]);
	styleset_get_hex(config, "styling", "hash", "0x905010", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[14]);
	styleset_get_hex(config, "styling", "symboltable", "0x905010", "0xffffff", "false", types[GEANY_FILETYPES_PERL]->styling[15]);
	styleset_get_hex(config, "styling", "backticks", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_PERL]->styling[16]);

	types[GEANY_FILETYPES_PERL]->keywords = g_new(gchar*, 2);
	types[GEANY_FILETYPES_PERL]->keywords[0] = styleset_get_string(config, "keywords", "primary");
	if (types[GEANY_FILETYPES_PERL]->keywords[0] == NULL)
			types[GEANY_FILETYPES_PERL]->keywords[0] = g_strdup(
									"NULL __FILE__ __LINE__ __PACKAGE__ __DATA__ __END__ AUTOLOAD \
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

	g_key_file_free(config);
}


void styleset_perl(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_PERL] == NULL) styleset_perl_init();

	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_PERL, 0);

	// enable folding for retrieval of current function (utils_get_current_tag)
	SSM (sci, SCI_SETPROPERTY, (sptr_t) "fold", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (sptr_t) "styling.within.preprocessor", (sptr_t) "1");

	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) GEANY_WORDCHARS);
	SSM (sci, SCI_AUTOCSETMAXHEIGHT, 8, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_PERL]->keywords[0]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_DEFAULT, types[GEANY_FILETYPES_PERL]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_DEFAULT, types[GEANY_FILETYPES_PERL]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_DEFAULT, types[GEANY_FILETYPES_PERL]->styling[0][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_ERROR, types[GEANY_FILETYPES_PERL]->styling[1][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_ERROR, types[GEANY_FILETYPES_PERL]->styling[1][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_ERROR, types[GEANY_FILETYPES_PERL]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_COMMENTLINE, types[GEANY_FILETYPES_PERL]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_COMMENTLINE, types[GEANY_FILETYPES_PERL]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_COMMENTLINE, types[GEANY_FILETYPES_PERL]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_NUMBER, types[GEANY_FILETYPES_PERL]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_NUMBER, types[GEANY_FILETYPES_PERL]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_NUMBER, types[GEANY_FILETYPES_PERL]->styling[3][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_WORD, types[GEANY_FILETYPES_PERL]->styling[4][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_WORD, types[GEANY_FILETYPES_PERL]->styling[4][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_WORD, types[GEANY_FILETYPES_PERL]->styling[4][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_STRING, types[GEANY_FILETYPES_PERL]->styling[5][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_STRING, types[GEANY_FILETYPES_PERL]->styling[5][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_STRING, types[GEANY_FILETYPES_PERL]->styling[5][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_CHARACTER, types[GEANY_FILETYPES_PERL]->styling[6][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_CHARACTER, types[GEANY_FILETYPES_PERL]->styling[6][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_CHARACTER, types[GEANY_FILETYPES_PERL]->styling[6][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_PREPROCESSOR, types[GEANY_FILETYPES_PERL]->styling[7][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_PREPROCESSOR, types[GEANY_FILETYPES_PERL]->styling[7][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_PREPROCESSOR, types[GEANY_FILETYPES_PERL]->styling[7][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_OPERATOR, types[GEANY_FILETYPES_PERL]->styling[8][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_OPERATOR, types[GEANY_FILETYPES_PERL]->styling[8][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_OPERATOR, types[GEANY_FILETYPES_PERL]->styling[8][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_IDENTIFIER, types[GEANY_FILETYPES_PERL]->styling[9][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_IDENTIFIER, types[GEANY_FILETYPES_PERL]->styling[9][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_IDENTIFIER, types[GEANY_FILETYPES_PERL]->styling[9][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_SCALAR, types[GEANY_FILETYPES_PERL]->styling[10][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_SCALAR, types[GEANY_FILETYPES_PERL]->styling[10][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_SCALAR, types[GEANY_FILETYPES_PERL]->styling[10][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_POD, types[GEANY_FILETYPES_PERL]->styling[11][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_POD, types[GEANY_FILETYPES_PERL]->styling[11][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_POD, types[GEANY_FILETYPES_PERL]->styling[11][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_REGEX, types[GEANY_FILETYPES_PERL]->styling[12][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_REGEX, types[GEANY_FILETYPES_PERL]->styling[12][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_REGEX, types[GEANY_FILETYPES_PERL]->styling[12][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_ARRAY, types[GEANY_FILETYPES_PERL]->styling[13][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_ARRAY, types[GEANY_FILETYPES_PERL]->styling[13][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_ARRAY, types[GEANY_FILETYPES_PERL]->styling[13][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_BACKTICKS, types[GEANY_FILETYPES_PERL]->styling[14][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_BACKTICKS, types[GEANY_FILETYPES_PERL]->styling[14][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_BACKTICKS, types[GEANY_FILETYPES_PERL]->styling[14][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_HASH, types[GEANY_FILETYPES_PERL]->styling[15][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_HASH, types[GEANY_FILETYPES_PERL]->styling[15][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_HASH, types[GEANY_FILETYPES_PERL]->styling[15][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PL_SYMBOLTABLE, types[GEANY_FILETYPES_PERL]->styling[16][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PL_SYMBOLTABLE, types[GEANY_FILETYPES_PERL]->styling[16][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PL_SYMBOLTABLE, types[GEANY_FILETYPES_PERL]->styling[16][2]);

	SSM(sci, SCI_SETWHITESPACEFORE, 1, 0xc0c0c0);
}


void styleset_python_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.python", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_PYTHON] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "default", "0xFF0000", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[0]);
	styleset_get_hex(config, "styling", "commentline", "0xFF0000", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[1]);
	styleset_get_hex(config, "styling", "number", "0x800040", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[2]);
	styleset_get_hex(config, "styling", "string", "0x008000", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[3]);
	styleset_get_hex(config, "styling", "character", "0x008000", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[4]);
	styleset_get_hex(config, "styling", "word", "0x800060", "0xffffff", "true", types[GEANY_FILETYPES_PYTHON]->styling[5]);
	styleset_get_hex(config, "styling", "triple", "0x208000", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[6]);
	styleset_get_hex(config, "styling", "tripledouble", "0x004040", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[7]);
	styleset_get_hex(config, "styling", "classname", "0x303000", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[8]);
	styleset_get_hex(config, "styling", "defname", "0x800000", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[9]);
	styleset_get_hex(config, "styling", "operator", "0x800030", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[10]);
	styleset_get_hex(config, "styling", "identifier", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[11]);
	styleset_get_hex(config, "styling", "commentblock", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_PYTHON]->styling[12]);
	styleset_get_hex(config, "styling", "stringeol", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_PYTHON]->styling[13]);

	types[GEANY_FILETYPES_PYTHON]->keywords = g_new(gchar*, 2);
	types[GEANY_FILETYPES_PYTHON]->keywords[0] = styleset_get_string(config, "keywords", "primary");
	if (types[GEANY_FILETYPES_PYTHON]->keywords[0] == NULL)
			types[GEANY_FILETYPES_PYTHON]->keywords[0] = g_strdup("and assert break class continue def del elif else except exec finally for from global if import in is lambda not or pass print raise return try while yield");
	types[GEANY_FILETYPES_PYTHON]->keywords[1] = NULL;

	g_key_file_free(config);
}


void styleset_python(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_PYTHON] == NULL) styleset_python_init();

	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_PYTHON, 0);

	// enable folding for retrieval of current function (utils_get_current_tag)
	SSM (sci, SCI_SETPROPERTY, (sptr_t) "fold", (sptr_t) "1");

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_PYTHON]->keywords[0]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_DEFAULT, types[GEANY_FILETYPES_PYTHON]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_DEFAULT, types[GEANY_FILETYPES_PYTHON]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_DEFAULT, types[GEANY_FILETYPES_PYTHON]->styling[0][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_COMMENTLINE, types[GEANY_FILETYPES_PYTHON]->styling[1][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_COMMENTLINE, types[GEANY_FILETYPES_PYTHON]->styling[1][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_COMMENTLINE, types[GEANY_FILETYPES_PYTHON]->styling[1][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_NUMBER, types[GEANY_FILETYPES_PYTHON]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_NUMBER, types[GEANY_FILETYPES_PYTHON]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_NUMBER, types[GEANY_FILETYPES_PYTHON]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_STRING, types[GEANY_FILETYPES_PYTHON]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_STRING, types[GEANY_FILETYPES_PYTHON]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_STRING, types[GEANY_FILETYPES_PYTHON]->styling[3][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_CHARACTER, types[GEANY_FILETYPES_PYTHON]->styling[4][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_CHARACTER, types[GEANY_FILETYPES_PYTHON]->styling[4][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_CHARACTER, types[GEANY_FILETYPES_PYTHON]->styling[4][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_WORD, types[GEANY_FILETYPES_PYTHON]->styling[5][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_WORD, types[GEANY_FILETYPES_PYTHON]->styling[5][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_WORD, types[GEANY_FILETYPES_PYTHON]->styling[5][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_TRIPLE, types[GEANY_FILETYPES_PYTHON]->styling[6][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_TRIPLE, types[GEANY_FILETYPES_PYTHON]->styling[6][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_TRIPLE, types[GEANY_FILETYPES_PYTHON]->styling[6][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_TRIPLEDOUBLE, types[GEANY_FILETYPES_PYTHON]->styling[7][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_TRIPLEDOUBLE, types[GEANY_FILETYPES_PYTHON]->styling[7][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_TRIPLEDOUBLE, types[GEANY_FILETYPES_PYTHON]->styling[7][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_CLASSNAME, types[GEANY_FILETYPES_PYTHON]->styling[8][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_CLASSNAME, types[GEANY_FILETYPES_PYTHON]->styling[8][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_CLASSNAME, types[GEANY_FILETYPES_PYTHON]->styling[8][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_DEFNAME, types[GEANY_FILETYPES_PYTHON]->styling[9][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_DEFNAME, types[GEANY_FILETYPES_PYTHON]->styling[9][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_DEFNAME, types[GEANY_FILETYPES_PYTHON]->styling[9][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_OPERATOR, types[GEANY_FILETYPES_PYTHON]->styling[10][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_OPERATOR, types[GEANY_FILETYPES_PYTHON]->styling[10][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_OPERATOR, types[GEANY_FILETYPES_PYTHON]->styling[10][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_IDENTIFIER, types[GEANY_FILETYPES_PYTHON]->styling[11][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_IDENTIFIER, types[GEANY_FILETYPES_PYTHON]->styling[11][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_IDENTIFIER, types[GEANY_FILETYPES_PYTHON]->styling[11][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_COMMENTBLOCK, types[GEANY_FILETYPES_PYTHON]->styling[12][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_COMMENTBLOCK, types[GEANY_FILETYPES_PYTHON]->styling[12][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_COMMENTBLOCK, types[GEANY_FILETYPES_PYTHON]->styling[12][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_P_STRINGEOL, types[GEANY_FILETYPES_PYTHON]->styling[13][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_P_STRINGEOL, types[GEANY_FILETYPES_PYTHON]->styling[13][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_P_STRINGEOL, types[GEANY_FILETYPES_PYTHON]->styling[13][2]);
}


void styleset_sh_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.sh", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_SH] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[0]);
	styleset_get_hex(config, "styling", "commentline", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[1]);
	styleset_get_hex(config, "styling", "number", "0x007F00", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[2]);
	styleset_get_hex(config, "styling", "word", "0x991111", "0xffffff", "true", types[GEANY_FILETYPES_SH]->styling[3]);
	styleset_get_hex(config, "styling", "string", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[4]);
	styleset_get_hex(config, "styling", "character", "0x004040", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[5]);
	styleset_get_hex(config, "styling", "operator", "0x101030", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[6]);
	styleset_get_hex(config, "styling", "identifier", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[7]);
	styleset_get_hex(config, "styling", "backticks", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_SH]->styling[8]);
	styleset_get_hex(config, "styling", "param", "0xF38A12", "0x0000ff", "false", types[GEANY_FILETYPES_SH]->styling[9]);
	styleset_get_hex(config, "styling", "scalar", "0x905010", "0xffffff", "false", types[GEANY_FILETYPES_SH]->styling[10]);

	types[GEANY_FILETYPES_SH]->keywords = g_new(gchar*, 2);
	types[GEANY_FILETYPES_SH]->keywords[0] = styleset_get_string(config, "keywords", "primary");
	if (types[GEANY_FILETYPES_SH]->keywords[0] == NULL)
			types[GEANY_FILETYPES_SH]->keywords[0] = g_strdup("break case continue do done else esac eval exit export fi for goto if in integer return set shift then while");
	types[GEANY_FILETYPES_SH]->keywords[1] = NULL;

	g_key_file_free(config);
}


void styleset_sh(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_SH] == NULL) styleset_sh_init();

	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_BASH, 0);

	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) GEANY_WORDCHARS);
	SSM (sci, SCI_AUTOCSETMAXHEIGHT, 8, 0);

	SSM (sci, SCI_SETCONTROLCHARSYMBOL, 32, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_SH]->keywords[0]);

	SSM(sci, SCI_STYLESETFORE, SCE_SH_DEFAULT, types[GEANY_FILETYPES_SH]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_SH_DEFAULT, types[GEANY_FILETYPES_SH]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_SH_DEFAULT, types[GEANY_FILETYPES_SH]->styling[0][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_SH_COMMENTLINE, types[GEANY_FILETYPES_SH]->styling[1][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_SH_COMMENTLINE, types[GEANY_FILETYPES_SH]->styling[1][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_SH_COMMENTLINE, types[GEANY_FILETYPES_SH]->styling[1][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_SH_NUMBER, types[GEANY_FILETYPES_SH]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_SH_NUMBER, types[GEANY_FILETYPES_SH]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_SH_NUMBER, types[GEANY_FILETYPES_SH]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_SH_WORD, types[GEANY_FILETYPES_SH]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_SH_WORD, types[GEANY_FILETYPES_SH]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_SH_WORD, types[GEANY_FILETYPES_SH]->styling[3][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_SH_STRING, types[GEANY_FILETYPES_SH]->styling[4][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_SH_STRING, types[GEANY_FILETYPES_SH]->styling[4][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_SH_STRING, types[GEANY_FILETYPES_SH]->styling[4][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_SH_CHARACTER, types[GEANY_FILETYPES_SH]->styling[5][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_SH_CHARACTER, types[GEANY_FILETYPES_SH]->styling[5][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_SH_CHARACTER, types[GEANY_FILETYPES_SH]->styling[5][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_SH_OPERATOR, types[GEANY_FILETYPES_SH]->styling[6][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_SH_OPERATOR, types[GEANY_FILETYPES_SH]->styling[6][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_SH_OPERATOR, types[GEANY_FILETYPES_SH]->styling[6][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_SH_IDENTIFIER, types[GEANY_FILETYPES_SH]->styling[7][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_SH_IDENTIFIER, types[GEANY_FILETYPES_SH]->styling[7][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_SH_IDENTIFIER, types[GEANY_FILETYPES_SH]->styling[7][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_SH_BACKTICKS, types[GEANY_FILETYPES_SH]->styling[8][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_SH_BACKTICKS, types[GEANY_FILETYPES_SH]->styling[8][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_SH_BACKTICKS, types[GEANY_FILETYPES_SH]->styling[8][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_SH_PARAM, types[GEANY_FILETYPES_SH]->styling[9][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_SH_PARAM, types[GEANY_FILETYPES_SH]->styling[9][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_SH_PARAM, types[GEANY_FILETYPES_SH]->styling[9][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_SH_SCALAR, types[GEANY_FILETYPES_SH]->styling[10][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_SH_SCALAR, types[GEANY_FILETYPES_SH]->styling[10][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_SH_SCALAR, types[GEANY_FILETYPES_SH]->styling[10][2]);

	SSM(sci, SCI_SETWHITESPACEFORE, 1, 0xc0c0c0);
}


void styleset_xml(ScintillaObject *sci)
{
	styleset_common(sci, 7);

	SSM (sci, SCI_SETLEXER, SCLEX_XML, 0);

	// use the same colouring for HTML; XML and so on
	styleset_markup(sci);
}


void styleset_docbook_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.docbook", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_DOCBOOK] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[0]);
	styleset_get_hex(config, "styling", "tag", "0x990000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[1]);
	styleset_get_hex(config, "styling", "tagunknown", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[2]);
	styleset_get_hex(config, "styling", "attribute", "0x007F00", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[3]);
	styleset_get_hex(config, "styling", "attributeunknown", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[4]);
	styleset_get_hex(config, "styling", "number", "0x800080", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[5]);
	styleset_get_hex(config, "styling", "doublestring", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[6]);
	styleset_get_hex(config, "styling", "singlestring", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[7]);
	styleset_get_hex(config, "styling", "other", "0x800080", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[8]);
	styleset_get_hex(config, "styling", "comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[9]);
	styleset_get_hex(config, "styling", "entity", "0x800080", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[10]);
	styleset_get_hex(config, "styling", "tagend", "0x990000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[11]);
	styleset_get_hex(config, "styling", "xmlstart", "0x990000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[12]);
	styleset_get_hex(config, "styling", "xmlend", "0x990000", "0xf0f0f0", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[13]);
	styleset_get_hex(config, "styling", "cdata", "0x990066", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[14]);
	styleset_get_hex(config, "styling", "question", "0xff0000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[15]);
	styleset_get_hex(config, "styling", "value", "0x990066", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[16]);
	styleset_get_hex(config, "styling", "xccomment", "0x990066", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[17]);
	styleset_get_hex(config, "styling", "sgml_default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[18]);
	styleset_get_hex(config, "styling", "sgml_comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[19]);
	styleset_get_hex(config, "styling", "sgml_special", "0x007F00", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[20]);
	styleset_get_hex(config, "styling", "sgml_command", "0x991111", "0xffffff", "true", types[GEANY_FILETYPES_DOCBOOK]->styling[21]);
	styleset_get_hex(config, "styling", "sgml_doublestring", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[22]);
	styleset_get_hex(config, "styling", "sgml_simplestring", "0x004040", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[23]);
	styleset_get_hex(config, "styling", "sgml_1st_param", "0x804040", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[24]);
	styleset_get_hex(config, "styling", "sgml_entity", "0x101030", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[25]);
	styleset_get_hex(config, "styling", "sgml_block_default", "0x100000", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[26]);
	styleset_get_hex(config, "styling", "sgml_1st_param_comment", "0x906040", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[27]);
	styleset_get_hex(config, "styling", "sgml_error", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_DOCBOOK]->styling[28]);

	types[GEANY_FILETYPES_DOCBOOK]->keywords = g_new(gchar*, 2);
	types[GEANY_FILETYPES_DOCBOOK]->keywords[0] = styleset_get_string(config, "keywords", "elements");
	if (types[GEANY_FILETYPES_DOCBOOK]->keywords[0] == NULL)
			types[GEANY_FILETYPES_DOCBOOK]->keywords[0] = g_strdup("\
			abbrev abstract accel ackno acronym action address affiliation alt anchor \
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
	types[GEANY_FILETYPES_DOCBOOK]->keywords[1] = styleset_get_string(config, "keywords", "dtd");
	if (types[GEANY_FILETYPES_DOCBOOK]->keywords[1] == NULL)
			types[GEANY_FILETYPES_DOCBOOK]->keywords[1] = g_strdup("ELEMENT DOCTYPE ATTLIST ENTITY NOTATION");
	types[GEANY_FILETYPES_DOCBOOK]->keywords[2] = NULL;

	g_key_file_free(config);
}


void styleset_docbook(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_DOCBOOK] == NULL) styleset_docbook_init();

	styleset_common(sci, 7);
	SSM (sci, SCI_SETLEXER, SCLEX_XML, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_DOCBOOK]->keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 5, (sptr_t) types[GEANY_FILETYPES_DOCBOOK]->keywords[1]);

	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) GEANY_WORDCHARS);
	SSM (sci, SCI_AUTOCSETMAXHEIGHT, 8, 0);

	// Unknown tags and attributes are highlighed in red.
	// If a tag is actually OK, it should be added in lower case to the htmlKeyWords string.

	SSM(sci, SCI_STYLESETFORE, SCE_H_DEFAULT, types[GEANY_FILETYPES_DOCBOOK]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_DEFAULT, types[GEANY_FILETYPES_DOCBOOK]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_DEFAULT, types[GEANY_FILETYPES_DOCBOOK]->styling[0][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_TAG, types[GEANY_FILETYPES_DOCBOOK]->styling[1][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_TAG, types[GEANY_FILETYPES_DOCBOOK]->styling[1][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_TAG, types[GEANY_FILETYPES_DOCBOOK]->styling[1][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_TAGUNKNOWN, types[GEANY_FILETYPES_DOCBOOK]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_TAGUNKNOWN, types[GEANY_FILETYPES_DOCBOOK]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_TAGUNKNOWN, types[GEANY_FILETYPES_DOCBOOK]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_ATTRIBUTE, types[GEANY_FILETYPES_DOCBOOK]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_ATTRIBUTE, types[GEANY_FILETYPES_DOCBOOK]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_ATTRIBUTE, types[GEANY_FILETYPES_DOCBOOK]->styling[3][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_ATTRIBUTEUNKNOWN, types[GEANY_FILETYPES_DOCBOOK]->styling[4][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_ATTRIBUTEUNKNOWN, types[GEANY_FILETYPES_DOCBOOK]->styling[4][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_ATTRIBUTEUNKNOWN, types[GEANY_FILETYPES_DOCBOOK]->styling[4][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_NUMBER, types[GEANY_FILETYPES_DOCBOOK]->styling[5][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_NUMBER, types[GEANY_FILETYPES_DOCBOOK]->styling[5][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_NUMBER, types[GEANY_FILETYPES_DOCBOOK]->styling[5][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_DOUBLESTRING, types[GEANY_FILETYPES_DOCBOOK]->styling[6][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_DOUBLESTRING, types[GEANY_FILETYPES_DOCBOOK]->styling[6][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_DOUBLESTRING, types[GEANY_FILETYPES_DOCBOOK]->styling[6][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SINGLESTRING, types[GEANY_FILETYPES_DOCBOOK]->styling[7][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SINGLESTRING, types[GEANY_FILETYPES_DOCBOOK]->styling[7][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SINGLESTRING, types[GEANY_FILETYPES_DOCBOOK]->styling[7][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_OTHER, types[GEANY_FILETYPES_DOCBOOK]->styling[8][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_OTHER, types[GEANY_FILETYPES_DOCBOOK]->styling[8][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_OTHER, types[GEANY_FILETYPES_DOCBOOK]->styling[8][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_COMMENT, types[GEANY_FILETYPES_DOCBOOK]->styling[9][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_COMMENT, types[GEANY_FILETYPES_DOCBOOK]->styling[9][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_COMMENT, types[GEANY_FILETYPES_DOCBOOK]->styling[9][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_ENTITY, types[GEANY_FILETYPES_DOCBOOK]->styling[10][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_ENTITY, types[GEANY_FILETYPES_DOCBOOK]->styling[10][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_ENTITY, types[GEANY_FILETYPES_DOCBOOK]->styling[10][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_TAGEND, types[GEANY_FILETYPES_DOCBOOK]->styling[11][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_TAGEND, types[GEANY_FILETYPES_DOCBOOK]->styling[11][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_TAGEND, types[GEANY_FILETYPES_DOCBOOK]->styling[11][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_XMLSTART, types[GEANY_FILETYPES_DOCBOOK]->styling[12][0]);// <?
	SSM(sci, SCI_STYLESETBACK, SCE_H_XMLSTART, types[GEANY_FILETYPES_DOCBOOK]->styling[12][1]);
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_H_XMLSTART, 1);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_XMLSTART, types[GEANY_FILETYPES_DOCBOOK]->styling[12][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_XMLEND, types[GEANY_FILETYPES_DOCBOOK]->styling[13][0]);// ?>
	SSM(sci, SCI_STYLESETBACK, SCE_H_XMLEND, types[GEANY_FILETYPES_DOCBOOK]->styling[13][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_XMLEND, types[GEANY_FILETYPES_DOCBOOK]->styling[13][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_CDATA, types[GEANY_FILETYPES_DOCBOOK]->styling[14][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_CDATA, types[GEANY_FILETYPES_DOCBOOK]->styling[14][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_CDATA, types[GEANY_FILETYPES_DOCBOOK]->styling[14][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_QUESTION, types[GEANY_FILETYPES_DOCBOOK]->styling[15][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_QUESTION, types[GEANY_FILETYPES_DOCBOOK]->styling[15][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_QUESTION, types[GEANY_FILETYPES_DOCBOOK]->styling[15][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_VALUE, types[GEANY_FILETYPES_DOCBOOK]->styling[16][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_VALUE, types[GEANY_FILETYPES_DOCBOOK]->styling[16][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_VALUE, types[GEANY_FILETYPES_DOCBOOK]->styling[16][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_XCCOMMENT, types[GEANY_FILETYPES_DOCBOOK]->styling[17][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_XCCOMMENT, types[GEANY_FILETYPES_DOCBOOK]->styling[17][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_XCCOMMENT, types[GEANY_FILETYPES_DOCBOOK]->styling[17][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_DEFAULT, types[GEANY_FILETYPES_DOCBOOK]->styling[18][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_DEFAULT, types[GEANY_FILETYPES_DOCBOOK]->styling[18][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_DEFAULT, types[GEANY_FILETYPES_DOCBOOK]->styling[18][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_COMMENT, types[GEANY_FILETYPES_DOCBOOK]->styling[19][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_COMMENT, types[GEANY_FILETYPES_DOCBOOK]->styling[19][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_COMMENT, types[GEANY_FILETYPES_DOCBOOK]->styling[19][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_SPECIAL, types[GEANY_FILETYPES_DOCBOOK]->styling[20][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_SPECIAL, types[GEANY_FILETYPES_DOCBOOK]->styling[20][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_SPECIAL, types[GEANY_FILETYPES_DOCBOOK]->styling[20][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_COMMAND, types[GEANY_FILETYPES_DOCBOOK]->styling[21][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_COMMAND, types[GEANY_FILETYPES_DOCBOOK]->styling[21][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_COMMAND, types[GEANY_FILETYPES_DOCBOOK]->styling[21][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_DOUBLESTRING, types[GEANY_FILETYPES_DOCBOOK]->styling[22][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_DOUBLESTRING, types[GEANY_FILETYPES_DOCBOOK]->styling[22][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_DOUBLESTRING, types[GEANY_FILETYPES_DOCBOOK]->styling[22][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_SIMPLESTRING, types[GEANY_FILETYPES_DOCBOOK]->styling[23][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_SIMPLESTRING, types[GEANY_FILETYPES_DOCBOOK]->styling[23][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_SIMPLESTRING, types[GEANY_FILETYPES_DOCBOOK]->styling[23][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_1ST_PARAM, types[GEANY_FILETYPES_DOCBOOK]->styling[24][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_1ST_PARAM, types[GEANY_FILETYPES_DOCBOOK]->styling[24][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_1ST_PARAM, types[GEANY_FILETYPES_DOCBOOK]->styling[24][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_ENTITY, types[GEANY_FILETYPES_DOCBOOK]->styling[25][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_ENTITY, types[GEANY_FILETYPES_DOCBOOK]->styling[25][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_ENTITY, types[GEANY_FILETYPES_DOCBOOK]->styling[25][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_BLOCK_DEFAULT, types[GEANY_FILETYPES_DOCBOOK]->styling[26][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_BLOCK_DEFAULT, types[GEANY_FILETYPES_DOCBOOK]->styling[26][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_BLOCK_DEFAULT, types[GEANY_FILETYPES_DOCBOOK]->styling[26][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_1ST_PARAM_COMMENT, types[GEANY_FILETYPES_DOCBOOK]->styling[27][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_1ST_PARAM_COMMENT, types[GEANY_FILETYPES_DOCBOOK]->styling[27][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_1ST_PARAM_COMMENT, types[GEANY_FILETYPES_DOCBOOK]->styling[27][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_H_SGML_ERROR, types[GEANY_FILETYPES_DOCBOOK]->styling[28][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_H_SGML_ERROR, types[GEANY_FILETYPES_DOCBOOK]->styling[28][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_H_SGML_ERROR, types[GEANY_FILETYPES_DOCBOOK]->styling[28][2]);

}


void styleset_none(ScintillaObject *sci)
{
	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_NULL, 0);
}


void styleset_css_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.css", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_CSS] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[0]);
	styleset_get_hex(config, "styling", "comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[1]);
	styleset_get_hex(config, "styling", "tag", "0x00007F", "0xffffff", "true", types[GEANY_FILETYPES_CSS]->styling[2]);
	styleset_get_hex(config, "styling", "class", "0x991111", "0xffffff", "true", types[GEANY_FILETYPES_CSS]->styling[3]);
	styleset_get_hex(config, "styling", "pseudoclass", "0x991111", "0xffffff", "true", types[GEANY_FILETYPES_CSS]->styling[4]);
	styleset_get_hex(config, "styling", "unknown_pseudoclass", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[5]);
	styleset_get_hex(config, "styling", "unknown_identifier", "0x0000ff", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[6]);
	styleset_get_hex(config, "styling", "operator", "0x101030", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[7]);
	styleset_get_hex(config, "styling", "identifier", "0x100000", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[8]);
	styleset_get_hex(config, "styling", "doublestring", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[9]);
	styleset_get_hex(config, "styling", "singlestring", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[10]);
	styleset_get_hex(config, "styling", "attribute", "0x007F00", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[11]);
	styleset_get_hex(config, "styling", "value", "0x7F7F00", "0xffffff", "false", types[GEANY_FILETYPES_CSS]->styling[12]);

	types[GEANY_FILETYPES_CSS]->keywords = g_new(gchar*, 4);
	types[GEANY_FILETYPES_CSS]->keywords[0] = styleset_get_string(config, "keywords", "primary");
	if (types[GEANY_FILETYPES_CSS]->keywords[0] == NULL)
			types[GEANY_FILETYPES_CSS]->keywords[0] = g_strdup("color background-color background-image background-repeat background-attachment background-position background \
								font-family font-style font-variant font-weight font-size font \
								word-spacing letter-spacing text-decoration vertical-align text-transform text-align text-indent line-height \
								margin-top margin-right margin-bottom margin-left margin \
								padding-top padding-right padding-bottom padding-left padding \
								border-top-width border-right-width border-bottom-width border-left-width border-width \
								border-top border-right border-bottom border-left border \
								border-color border-style width height float clear \
								display white-space list-style-type list-style-image list-style-position list-style");
	types[GEANY_FILETYPES_CSS]->keywords[1] = styleset_get_string(config, "keywords", "secondary");
	if (types[GEANY_FILETYPES_CSS]->keywords[1] == NULL)
			types[GEANY_FILETYPES_CSS]->keywords[1] = g_strdup("border-top-color border-right-color border-bottom-color border-left-color border-color \
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
	types[GEANY_FILETYPES_CSS]->keywords[2] = styleset_get_string(config, "keywords", "pseudoclasses");
	if (types[GEANY_FILETYPES_CSS]->keywords[2] == NULL)
			types[GEANY_FILETYPES_CSS]->keywords[2] = g_strdup("first-letter first-line link active visited lang first-child focus hover before after left right first");
	types[GEANY_FILETYPES_CSS]->keywords[3] = NULL;

	g_key_file_free(config);
}


void styleset_css(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_CSS] == NULL) styleset_css_init();

	styleset_common(sci, 5);

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) GEANY_WORDCHARS);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, 8, 0);

	SSM(sci, SCI_SETLEXER, SCLEX_CSS, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_CSS]->keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) types[GEANY_FILETYPES_CSS]->keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) types[GEANY_FILETYPES_CSS]->keywords[2]);

	SSM(sci, SCI_STYLESETFORE, SCE_CSS_DEFAULT, types[GEANY_FILETYPES_CSS]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_CSS_DEFAULT, types[GEANY_FILETYPES_CSS]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_CSS_DEFAULT, types[GEANY_FILETYPES_CSS]->styling[0][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_CSS_COMMENT, types[GEANY_FILETYPES_CSS]->styling[1][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_CSS_COMMENT, types[GEANY_FILETYPES_CSS]->styling[1][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_CSS_COMMENT, types[GEANY_FILETYPES_CSS]->styling[1][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_CSS_TAG, types[GEANY_FILETYPES_CSS]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_CSS_TAG, types[GEANY_FILETYPES_CSS]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_CSS_TAG, types[GEANY_FILETYPES_CSS]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_CSS_CLASS, types[GEANY_FILETYPES_CSS]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_CSS_CLASS, types[GEANY_FILETYPES_CSS]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_CSS_CLASS, types[GEANY_FILETYPES_CSS]->styling[3][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_CSS_PSEUDOCLASS, types[GEANY_FILETYPES_CSS]->styling[4][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_CSS_PSEUDOCLASS, types[GEANY_FILETYPES_CSS]->styling[4][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_CSS_PSEUDOCLASS, types[GEANY_FILETYPES_CSS]->styling[4][2]);
	SSM(sci, SCI_STYLESETITALIC, SCE_CSS_PSEUDOCLASS, TRUE);

	SSM(sci, SCI_STYLESETFORE, SCE_CSS_UNKNOWN_PSEUDOCLASS, types[GEANY_FILETYPES_CSS]->styling[5][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_CSS_UNKNOWN_PSEUDOCLASS, types[GEANY_FILETYPES_CSS]->styling[5][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_CSS_UNKNOWN_PSEUDOCLASS, types[GEANY_FILETYPES_CSS]->styling[5][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_CSS_UNKNOWN_IDENTIFIER, types[GEANY_FILETYPES_CSS]->styling[6][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_CSS_UNKNOWN_IDENTIFIER, types[GEANY_FILETYPES_CSS]->styling[6][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_CSS_UNKNOWN_IDENTIFIER, types[GEANY_FILETYPES_CSS]->styling[6][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_CSS_OPERATOR, types[GEANY_FILETYPES_CSS]->styling[7][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_CSS_OPERATOR, types[GEANY_FILETYPES_CSS]->styling[7][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_CSS_OPERATOR, types[GEANY_FILETYPES_CSS]->styling[7][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_CSS_IDENTIFIER, types[GEANY_FILETYPES_CSS]->styling[8][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_CSS_IDENTIFIER, types[GEANY_FILETYPES_CSS]->styling[8][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_CSS_IDENTIFIER, types[GEANY_FILETYPES_CSS]->styling[8][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_CSS_DOUBLESTRING, types[GEANY_FILETYPES_CSS]->styling[9][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_CSS_DOUBLESTRING, types[GEANY_FILETYPES_CSS]->styling[9][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_CSS_DOUBLESTRING, types[GEANY_FILETYPES_CSS]->styling[9][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_CSS_SINGLESTRING, types[GEANY_FILETYPES_CSS]->styling[10][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_CSS_SINGLESTRING, types[GEANY_FILETYPES_CSS]->styling[10][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_CSS_SINGLESTRING, types[GEANY_FILETYPES_CSS]->styling[10][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_CSS_ATTRIBUTE, types[GEANY_FILETYPES_CSS]->styling[11][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_CSS_ATTRIBUTE, types[GEANY_FILETYPES_CSS]->styling[11][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_CSS_ATTRIBUTE, types[GEANY_FILETYPES_CSS]->styling[11][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_CSS_VALUE, types[GEANY_FILETYPES_CSS]->styling[12][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_CSS_VALUE, types[GEANY_FILETYPES_CSS]->styling[12][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_CSS_VALUE, types[GEANY_FILETYPES_CSS]->styling[12][2]);
}


void styleset_conf_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.conf", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_CONF] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "default", "0x00007f", "0xffffff", "false", types[GEANY_FILETYPES_CONF]->styling[0]);
	styleset_get_hex(config, "styling", "comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_CONF]->styling[1]);
	styleset_get_hex(config, "styling", "section", "0x900000", "0xffffff", "true", types[GEANY_FILETYPES_CONF]->styling[2]);
	styleset_get_hex(config, "styling", "assignment", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_CONF]->styling[3]);
	styleset_get_hex(config, "styling", "defval", "0x7f0000", "0xffffff", "false", types[GEANY_FILETYPES_CONF]->styling[4]);

	types[GEANY_FILETYPES_CONF]->keywords = NULL;

	g_key_file_free(config);
}


void styleset_conf(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_CONF] == NULL) styleset_conf_init();

	styleset_common(sci, 5);
	SSM (sci, SCI_SETLEXER, SCLEX_PROPERTIES, 0);

	SSM (sci, SCI_SETWORDCHARS, 0, (sptr_t) GEANY_WORDCHARS);
	SSM (sci, SCI_AUTOCSETMAXHEIGHT, 8, 0);

	SSM (sci, SCI_SETCONTROLCHARSYMBOL, 32, 0);

	SSM(sci, SCI_STYLESETFORE, SCE_PROPS_DEFAULT, types[GEANY_FILETYPES_CONF]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PROPS_DEFAULT, types[GEANY_FILETYPES_CONF]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PROPS_DEFAULT, types[GEANY_FILETYPES_CONF]->styling[0][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PROPS_COMMENT, types[GEANY_FILETYPES_CONF]->styling[1][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PROPS_COMMENT, types[GEANY_FILETYPES_CONF]->styling[1][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PROPS_COMMENT, types[GEANY_FILETYPES_CONF]->styling[1][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PROPS_SECTION, types[GEANY_FILETYPES_CONF]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PROPS_SECTION, types[GEANY_FILETYPES_CONF]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PROPS_SECTION, types[GEANY_FILETYPES_CONF]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PROPS_ASSIGNMENT, types[GEANY_FILETYPES_CONF]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PROPS_ASSIGNMENT, types[GEANY_FILETYPES_CONF]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PROPS_ASSIGNMENT, types[GEANY_FILETYPES_CONF]->styling[3][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_PROPS_DEFVAL, types[GEANY_FILETYPES_CONF]->styling[4][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_PROPS_DEFVAL, types[GEANY_FILETYPES_CONF]->styling[4][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_PROPS_DEFVAL, types[GEANY_FILETYPES_CONF]->styling[4][2]);
}




void styleset_asm_init(void)
{
	GKeyFile *config = g_key_file_new();

	style_set_load_file(config, GEANY_DATA_DIR "/filetypes.asm", G_KEY_FILE_KEEP_COMMENTS, NULL);

	types[GEANY_FILETYPES_ASM] = g_new(style_set, 1);
	styleset_get_hex(config, "styling", "default", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[0]);
	styleset_get_hex(config, "styling", "comment", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[1]);
	styleset_get_hex(config, "styling", "number", "0x007f00", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[2]);
	styleset_get_hex(config, "styling", "string", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[3]);
	styleset_get_hex(config, "styling", "operator", "0x000000", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[4]);
	styleset_get_hex(config, "styling", "identifier", "0x000088", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[5]);
	styleset_get_hex(config, "styling", "cpuinstruction", "0x991111", "0xffffff", "true", types[GEANY_FILETYPES_ASM]->styling[6]);
	styleset_get_hex(config, "styling", "mathinstruction", "0x00007F", "0xffffff", "true", types[GEANY_FILETYPES_ASM]->styling[7]);
	styleset_get_hex(config, "styling", "register", "0x100000", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[8]);
	styleset_get_hex(config, "styling", "directive", "0x0F673D", "0xffffff", "true", types[GEANY_FILETYPES_ASM]->styling[9]);
	styleset_get_hex(config, "styling", "directiveoperand", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[10]);
	styleset_get_hex(config, "styling", "commentblock", "0x808080", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[11]);
	styleset_get_hex(config, "styling", "character", "0x1E90FF", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[12]);
	styleset_get_hex(config, "styling", "stringeol", "0x000000", "0xe0c0e0", "false", types[GEANY_FILETYPES_ASM]->styling[13]);
	styleset_get_hex(config, "styling", "extinstruction", "0x7F7F00", "0xffffff", "false", types[GEANY_FILETYPES_ASM]->styling[14]);

	types[GEANY_FILETYPES_ASM]->keywords = g_new(gchar*, 4);
	types[GEANY_FILETYPES_ASM]->keywords[0] = styleset_get_string(config, "keywords", "instructions");
	if (types[GEANY_FILETYPES_ASM]->keywords[0] == NULL)
			types[GEANY_FILETYPES_ASM]->keywords[0] = g_strdup("HLT LAD SPI ADD SUB MUL DIV JMP JEZ JGZ JLZ SWAP JSR RET PUSHAC POPAC ADDST SUBST MULST DIVST LSA LDS PUSH POP CLI LDI INK LIA DEK LDX");
	types[GEANY_FILETYPES_ASM]->keywords[1] = styleset_get_string(config, "keywords", "registers");
	if (types[GEANY_FILETYPES_ASM]->keywords[1] == NULL)
			types[GEANY_FILETYPES_ASM]->keywords[1] = g_strdup("");
	types[GEANY_FILETYPES_ASM]->keywords[2] = styleset_get_string(config, "keywords", "directives");
	if (types[GEANY_FILETYPES_ASM]->keywords[2] == NULL)
			types[GEANY_FILETYPES_ASM]->keywords[2] = g_strdup("ORG LIST NOLIST PAGE EQUIVALENT WORD TEXT");
	types[GEANY_FILETYPES_ASM]->keywords[3] = NULL;

	g_key_file_free(config);
}


void styleset_asm(ScintillaObject *sci)
{
	if (types[GEANY_FILETYPES_ASM] == NULL) styleset_asm_init();

	styleset_common(sci, 5);

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) GEANY_WORDCHARS);
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, 8, 0);

	SSM(sci, SCI_SETLEXER, SCLEX_ASM, 0);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) types[GEANY_FILETYPES_ASM]->keywords[0]);
	//SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) types[GEANY_FILETYPES_ASM]->keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) types[GEANY_FILETYPES_ASM]->keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 3, (sptr_t) types[GEANY_FILETYPES_ASM]->keywords[2]);
	//SSM(sci, SCI_SETKEYWORDS, 5, (sptr_t) types[GEANY_FILETYPES_ASM]->keywords[0]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_DEFAULT, types[GEANY_FILETYPES_ASM]->styling[0][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_DEFAULT, types[GEANY_FILETYPES_ASM]->styling[0][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_DEFAULT, types[GEANY_FILETYPES_ASM]->styling[0][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_COMMENT, types[GEANY_FILETYPES_ASM]->styling[1][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_COMMENT, types[GEANY_FILETYPES_ASM]->styling[1][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_COMMENT, types[GEANY_FILETYPES_ASM]->styling[1][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_NUMBER, types[GEANY_FILETYPES_ASM]->styling[2][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_NUMBER, types[GEANY_FILETYPES_ASM]->styling[2][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_NUMBER, types[GEANY_FILETYPES_ASM]->styling[2][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_STRING, types[GEANY_FILETYPES_ASM]->styling[3][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_STRING, types[GEANY_FILETYPES_ASM]->styling[3][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_STRING, types[GEANY_FILETYPES_ASM]->styling[3][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_OPERATOR, types[GEANY_FILETYPES_ASM]->styling[4][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_OPERATOR, types[GEANY_FILETYPES_ASM]->styling[4][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_OPERATOR, types[GEANY_FILETYPES_ASM]->styling[4][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_IDENTIFIER, types[GEANY_FILETYPES_ASM]->styling[5][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_IDENTIFIER, types[GEANY_FILETYPES_ASM]->styling[5][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_IDENTIFIER, types[GEANY_FILETYPES_ASM]->styling[5][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_CPUINSTRUCTION, types[GEANY_FILETYPES_ASM]->styling[6][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_CPUINSTRUCTION, types[GEANY_FILETYPES_ASM]->styling[6][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_CPUINSTRUCTION, types[GEANY_FILETYPES_ASM]->styling[6][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_MATHINSTRUCTION, types[GEANY_FILETYPES_ASM]->styling[7][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_MATHINSTRUCTION, types[GEANY_FILETYPES_ASM]->styling[7][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_MATHINSTRUCTION, types[GEANY_FILETYPES_ASM]->styling[7][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_REGISTER, types[GEANY_FILETYPES_ASM]->styling[8][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_REGISTER, types[GEANY_FILETYPES_ASM]->styling[8][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_REGISTER, types[GEANY_FILETYPES_ASM]->styling[8][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_DIRECTIVE, types[GEANY_FILETYPES_ASM]->styling[9][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_DIRECTIVE, types[GEANY_FILETYPES_ASM]->styling[9][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_DIRECTIVE, types[GEANY_FILETYPES_ASM]->styling[9][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_DIRECTIVEOPERAND, types[GEANY_FILETYPES_ASM]->styling[10][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_DIRECTIVEOPERAND, types[GEANY_FILETYPES_ASM]->styling[10][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_DIRECTIVEOPERAND, types[GEANY_FILETYPES_ASM]->styling[10][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_COMMENTBLOCK, types[GEANY_FILETYPES_ASM]->styling[11][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_COMMENTBLOCK, types[GEANY_FILETYPES_ASM]->styling[11][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_COMMENTBLOCK, types[GEANY_FILETYPES_ASM]->styling[11][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_CHARACTER, types[GEANY_FILETYPES_ASM]->styling[12][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_CHARACTER, types[GEANY_FILETYPES_ASM]->styling[12][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_CHARACTER, types[GEANY_FILETYPES_ASM]->styling[12][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_STRINGEOL, types[GEANY_FILETYPES_ASM]->styling[13][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_STRINGEOL, types[GEANY_FILETYPES_ASM]->styling[13][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_STRINGEOL, types[GEANY_FILETYPES_ASM]->styling[13][2]);

	SSM(sci, SCI_STYLESETFORE, SCE_ASM_EXTINSTRUCTION, types[GEANY_FILETYPES_ASM]->styling[14][0]);
	SSM(sci, SCI_STYLESETBACK, SCE_ASM_EXTINSTRUCTION, types[GEANY_FILETYPES_ASM]->styling[14][1]);
	SSM(sci, SCI_STYLESETBOLD, SCE_ASM_EXTINSTRUCTION, types[GEANY_FILETYPES_ASM]->styling[14][2]);
}


