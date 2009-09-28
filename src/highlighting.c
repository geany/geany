/*
 *      highlighting.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/**
 * @file highlighting.h
 * Syntax highlighting for the different filetypes, using the Scintilla lexers.
 */

#include "geany.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "SciLexer.h"
#include "highlighting.h"
#include "editor.h"
#include "utils.h"
#include "filetypes.h"
#include "symbols.h"
#include "ui_utils.h"
#include "utils.h"


/* Note: Avoid using SSM in files not related to scintilla, use sciwrappers.h instead. */
#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)

/* Whitespace has to be set after setting wordchars. */
#define GEANY_WHITESPACE_CHARS " \t" "!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~"

static gchar *whitespace_chars;

static void styleset_markup(ScintillaObject *sci, gboolean set_keywords);


typedef struct
{
	gsize			count;		/* number of styles */
	GeanyLexerStyle	*styling;		/* array of styles, NULL if not used or uninitialised */
	gchar			**keywords;
	gchar			*wordchars;	/* NULL used for style sets with no styles */
} StyleSet;

/* each filetype has a styleset except GEANY_FILETYPES_NONE, which uses common_style_set */
static StyleSet *style_sets = NULL;


enum	/* Geany common styling */
{
	GCS_DEFAULT,
	GCS_SELECTION,
	GCS_BRACE_GOOD,
	GCS_BRACE_BAD,
	GCS_MARGIN_LINENUMBER,
	GCS_MARGIN_FOLDING,
	GCS_CURRENT_LINE,
	GCS_CARET,
	GCS_INDENT_GUIDE,
	GCS_WHITE_SPACE,
	GCS_LINE_WRAP_VISUALS,
	GCS_LINE_WRAP_INDENT,
	GCS_TRANSLUCENCY,
	GCS_MARKER_LINE,
	GCS_MARKER_SEARCH,
	GCS_MARKER_MARK,
	GCS_MARKER_TRANSLUCENCY,
	GCS_LINE_HEIGHT,
	GCS_MAX
};

static struct
{
	GeanyLexerStyle	 styling[GCS_MAX];

	/* can take values 1 or 2 (or 3) */
	gint fold_marker;
	gint fold_lines;
	gint fold_draw_line;

	gchar			*wordchars;
} common_style_set;


/* For filetypes.common [named_styles] section.
 * 0xBBGGRR format.
 * e.g. "comment" => &GeanyLexerStyle{0x0000d0, 0xffffff, FALSE, FALSE} */
static GHashTable *named_style_hash = NULL;

/* 0xBBGGRR format, set by "default" named style. */
static GeanyLexerStyle gsd_default = {0x000000, 0xffffff, FALSE, FALSE};


static void new_styleset(gint file_type_id, gint styling_count)
{
	StyleSet *set = &style_sets[file_type_id];

	set->count = styling_count;
	set->styling = g_new0(GeanyLexerStyle, styling_count);
}


static void free_styleset(gint file_type_id)
{
	StyleSet *style_ptr;
	style_ptr = &style_sets[file_type_id];

	style_ptr->count = 0;
	g_free(style_ptr->styling);
	style_ptr->styling = NULL;
	g_strfreev(style_ptr->keywords);
	style_ptr->keywords = NULL;
	g_free(style_ptr->wordchars);
	style_ptr->wordchars = NULL;
}


static void get_keyfile_keywords(GKeyFile *config, GKeyFile *configh,
				const gchar *key, gint ft_id, gint pos)
{
	const gchar section[] = "keywords";
	gchar *result;
	const gchar *default_value = "";

	if (config == NULL || configh == NULL)
	{
		style_sets[ft_id].keywords[pos] = g_strdup(default_value);
		return;
	}

	result = g_key_file_get_string(configh, section, key, NULL);
	if (result == NULL)
		result = g_key_file_get_string(config, section, key, NULL);

	if (result == NULL)
	{
		style_sets[ft_id].keywords[pos] = g_strdup(default_value);
	}
	else
	{
		style_sets[ft_id].keywords[pos] = result;
	}
}


static void get_keyfile_wordchars(GKeyFile *config, GKeyFile *configh, gchar **wordchars)
{
	gchar *result;

	if (config == NULL || configh == NULL)
	{
		*wordchars = g_strdup(GEANY_WORDCHARS);
		return;
	}

	result = g_key_file_get_string(configh, "settings", "wordchars", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "settings", "wordchars", NULL);

	if (result == NULL)
	{
		*wordchars = g_strdup(GEANY_WORDCHARS);
	}
	else
		*wordchars = result;
}


static void read_named_style(const gchar *named_style, GeanyLexerStyle *style)
{
	GeanyLexerStyle *cs;
	gchar *comma, *name = NULL;
	const gchar *bold = NULL;
	const gchar *italic = NULL;

	g_return_if_fail(named_style);
	name = utils_strdupa(named_style);	/* named_style must not be written to, may be a static string */

	comma = strstr(name, ",");
	if (comma)
	{
		bold = strstr(comma, ",bold");
		italic = strstr(comma, ",italic");
		*comma = '\0';	/* terminate name to make lookup work */
	}
	cs = g_hash_table_lookup(named_style_hash, name);

	if (cs)
	{
 		*style = *cs;
 		if (bold)
 			style->bold = !style->bold;
 		if (italic)
 			style->italic = !style->italic;
	}
	else
	{
		*style = gsd_default;
		geany_debug("No named style '%s'! Check filetypes.common.", name);
	}
}


static void parse_color(const gchar *str, gint *clr)
{
	gint c;

	/* ignore empty strings */
	if (!NZV(str))
		return;

	c = utils_strtod(str, NULL, FALSE);
	if (c > -1)
	{
		*clr = c;
		return;
	}
	geany_debug("Bad color '%s'", str);
}


static void parse_keyfile_style(gchar **list,
		const GeanyLexerStyle *default_style, GeanyLexerStyle *style)
{
	gsize len;
	gchar *str;

	g_return_if_fail(default_style);
	g_return_if_fail(style);

	*style = *default_style;

	if (!list)
		return;

	len = g_strv_length(list);

	str = list[0];
	if (len == 1 && isalpha(str[0]))
		read_named_style(str, style);
	else
	{
		switch (len)
		{
			case 4:
				style->italic = utils_atob(list[3]);
			case 3:
				style->bold = utils_atob(list[2]);
			case 2:
				parse_color(list[1], &style->background);
			case 1:
				parse_color(list[0], &style->foreground);
		}
	}
}


static void get_keyfile_style(GKeyFile *config, GKeyFile *configh,
		const gchar *key_name, GeanyLexerStyle *style)
{
	gchar **list;
	gsize len;

	g_return_if_fail(config);
	g_return_if_fail(configh);
	g_return_if_fail(key_name);
	g_return_if_fail(style);

	list = g_key_file_get_string_list(configh, "styling", key_name, &len, NULL);
	if (list == NULL)
		list = g_key_file_get_string_list(config, "styling", key_name, &len, NULL);

	parse_keyfile_style(list, &gsd_default, style);
	g_strfreev(list);
}


/* Convert 0xRRGGBB to 0xBBGGRR, which scintilla expects. */
static gint rotate_rgb(gint color)
{
	return ((color & 0xFF0000) >> 16) +
		(color & 0x00FF00) +
		((color & 0x0000FF) << 16);
}


static void convert_int(const gchar *int_str, gint *val)
{
	gchar *end;
	gint v = strtol(int_str, &end, 10);

	if (int_str != end)
		*val = v;
}


/* Get first and second integer numbers, store in foreground and background fields of @a style. */
static void get_keyfile_int(GKeyFile *config, GKeyFile *configh, const gchar *section,
							const gchar *key, gint fdefault_val, gint sdefault_val,
							GeanyLexerStyle *style)
{
	gchar **list;
	gsize len;
	GeanyLexerStyle def = {fdefault_val, sdefault_val, FALSE, FALSE};

	g_return_if_fail(config);
	g_return_if_fail(configh);
	g_return_if_fail(section);
	g_return_if_fail(key);

	list = g_key_file_get_string_list(configh, section, key, &len, NULL);
	if (list == NULL)
		list = g_key_file_get_string_list(config, section, key, &len, NULL);

	*style = def;
	if (!list)
		return;

	if (list[0])
	{
		convert_int(list[0], &style->foreground);
		if (list[1])
		{
			convert_int(list[1], &style->background);
		}
	}
	g_strfreev(list);
}


/* first or second can be NULL. */
static void get_keyfile_ints(GKeyFile *config, GKeyFile *configh, const gchar *section,
							const gchar *key,
							gint fdefault_val, gint sdefault_val,
							gint *first, gint *second)
{
	GeanyLexerStyle tmp_style;

	get_keyfile_int(config, configh, section, key, fdefault_val, sdefault_val, &tmp_style);
	if (first)
		*first = tmp_style.foreground;
	if (second)
		*second = tmp_style.background;
}


static guint invert(guint icolour)
{
	if (interface_prefs.highlighting_invert_all)
		return utils_invert_color(icolour);

	return icolour;
}


static GeanyLexerStyle *get_style(guint ft_id, guint styling_index)
{
	g_assert(ft_id < filetypes_array->len);

	if (G_UNLIKELY(ft_id == GEANY_FILETYPES_NONE))
	{
		g_assert(styling_index < GCS_MAX);
		return &common_style_set.styling[styling_index];
	}
	else
	{
		StyleSet *set = &style_sets[ft_id];

		g_assert(styling_index < set->count);
		return &set->styling[styling_index];
	}
}


static void set_sci_style(ScintillaObject *sci, gint style, guint ft_id, guint styling_index)
{
	GeanyLexerStyle *style_ptr = get_style(ft_id, styling_index);

	SSM(sci, SCI_STYLESETFORE, style,	invert(style_ptr->foreground));
	SSM(sci, SCI_STYLESETBACK, style,	invert(style_ptr->background));
	SSM(sci, SCI_STYLESETBOLD, style,	style_ptr->bold);
	SSM(sci, SCI_STYLESETITALIC, style,	style_ptr->italic);
}


void highlighting_free_styles()
{
	guint i;

	for (i = 0; i < filetypes_array->len; i++)
		free_styleset(i);

	if (named_style_hash)
		g_hash_table_destroy(named_style_hash);

	if (style_sets)
		g_free(style_sets);
}


static GString *get_global_typenames(gint lang)
{
	GString *s = NULL;

	if (app->tm_workspace)
	{
		GPtrArray *tags_array = app->tm_workspace->global_tags;

		if (tags_array)
		{
			s = symbols_find_tags_as_string(tags_array, TM_GLOBAL_TYPE_MASK, lang);
		}
	}
	return s;
}


static gchar*
get_keyfile_whitespace_chars(GKeyFile *config, GKeyFile *configh)
{
	gchar *result;

	if (config == NULL || configh == NULL)
	{
		result = NULL;
	}
	else
	{
		result = g_key_file_get_string(configh, "settings", "whitespace_chars", NULL);
		if (result == NULL)
			result = g_key_file_get_string(config, "settings", "whitespace_chars", NULL);
	}
	if (result == NULL)
		result = g_strdup(GEANY_WHITESPACE_CHARS);
	return result;
}


static void add_named_style(GKeyFile *config, const gchar *key)
{
	const gchar group[] = "named_styles";
	gchar **list;
	gsize len;

	list = g_key_file_get_string_list(config, group, key, &len, NULL);
	/* we allow a named style to reference another style above it */
	if (list && len >= 1)
	{
		GeanyLexerStyle *style = g_new0(GeanyLexerStyle, 1);

		parse_keyfile_style(list, &gsd_default, style);
		g_hash_table_insert(named_style_hash, g_strdup(key), style);
	}
	g_strfreev(list);
}


static void get_named_styles(GKeyFile *config)
{
	const gchar group[] = "named_styles";
	gchar **keys = g_key_file_get_keys(config, group, NULL, NULL);
	gchar **ptr = keys;

	if (!ptr)
		return;

	while (1)
	{
		const gchar *key = *ptr;

		if (!key)
			break;

		/* don't replace already read default style with system one */
		if (!g_str_equal(key, "default"))
			add_named_style(config, key);

		ptr++;
	}
	g_strfreev(keys);
}


static void styleset_common_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	/* named styles */
	if (named_style_hash)
		g_hash_table_destroy(named_style_hash);	/* reloading */

	named_style_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	/* first set default to the "default" named style */
	add_named_style(config, "default");
	add_named_style(config_home, "default");
	read_named_style("default", &gsd_default);

	get_named_styles(config);
	/* home overrides any system named style */
	get_named_styles(config_home);

	get_keyfile_style(config, config_home, "default", &common_style_set.styling[GCS_DEFAULT]);
	get_keyfile_style(config, config_home, "selection", &common_style_set.styling[GCS_SELECTION]);
	get_keyfile_style(config, config_home, "brace_good", &common_style_set.styling[GCS_BRACE_GOOD]);
	get_keyfile_style(config, config_home, "brace_bad", &common_style_set.styling[GCS_BRACE_BAD]);
	get_keyfile_style(config, config_home, "margin_linenumber", &common_style_set.styling[GCS_MARGIN_LINENUMBER]);
	get_keyfile_style(config, config_home, "margin_folding", &common_style_set.styling[GCS_MARGIN_FOLDING]);
	get_keyfile_style(config, config_home, "current_line", &common_style_set.styling[GCS_CURRENT_LINE]);
	get_keyfile_style(config, config_home, "caret", &common_style_set.styling[GCS_CARET]);
	get_keyfile_style(config, config_home, "indent_guide", &common_style_set.styling[GCS_INDENT_GUIDE]);
	get_keyfile_style(config, config_home, "white_space", &common_style_set.styling[GCS_WHITE_SPACE]);
	get_keyfile_style(config, config_home, "marker_line", &common_style_set.styling[GCS_MARKER_LINE]);
	get_keyfile_style(config, config_home, "marker_search", &common_style_set.styling[GCS_MARKER_SEARCH]);
	get_keyfile_style(config, config_home, "marker_mark", &common_style_set.styling[GCS_MARKER_MARK]);

	get_keyfile_ints(config, config_home, "styling", "folding_style",
		1, 1, &common_style_set.fold_marker, &common_style_set.fold_lines);
	get_keyfile_ints(config, config_home, "styling", "folding_horiz_line",
		2, 0, &common_style_set.fold_draw_line, NULL);
	get_keyfile_ints(config, config_home, "styling", "caret_width",
		1, 0, &common_style_set.styling[GCS_CARET].background, NULL); /* caret.foreground used earlier */
	get_keyfile_int(config, config_home, "styling", "line_wrap_visuals",
		3, 0, &common_style_set.styling[GCS_LINE_WRAP_VISUALS]);
	get_keyfile_int(config, config_home, "styling", "line_wrap_indent",
		0, 0, &common_style_set.styling[GCS_LINE_WRAP_INDENT]);
	get_keyfile_int(config, config_home, "styling", "translucency",
		256, 256, &common_style_set.styling[GCS_TRANSLUCENCY]);
	get_keyfile_int(config, config_home, "styling", "marker_translucency",
		256, 256, &common_style_set.styling[GCS_MARKER_TRANSLUCENCY]);
	get_keyfile_int(config, config_home, "styling", "line_height",
		0, 0, &common_style_set.styling[GCS_LINE_HEIGHT]);

	get_keyfile_wordchars(config, config_home, &common_style_set.wordchars);
	whitespace_chars = get_keyfile_whitespace_chars(config, config_home);
}


static void styleset_common(ScintillaObject *sci, filetype_id ft_id)
{
	SSM(sci, SCI_STYLECLEARALL, 0, 0);

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) (ft_id == GEANY_FILETYPES_NONE ?
		common_style_set.wordchars : style_sets[ft_id].wordchars));
	/* have to set whitespace after setting wordchars */
	SSM(sci, SCI_SETWHITESPACECHARS, 0, (sptr_t) whitespace_chars);

	/* caret colour, style and width */
	SSM(sci, SCI_SETCARETFORE, invert(common_style_set.styling[GCS_CARET].foreground), 0);
	SSM(sci, SCI_SETCARETWIDTH, common_style_set.styling[GCS_CARET].background, 0);
	if (common_style_set.styling[GCS_CARET].bold)
		SSM(sci, SCI_SETCARETSTYLE, CARETSTYLE_BLOCK, 0);
	else
		SSM(sci, SCI_SETCARETSTYLE, CARETSTYLE_LINE, 0);

	/* line height */
	SSM(sci, SCI_SETEXTRAASCENT, common_style_set.styling[GCS_LINE_HEIGHT].foreground, 0);
	SSM(sci, SCI_SETEXTRADESCENT, common_style_set.styling[GCS_LINE_HEIGHT].background, 0);

	/* colourise the current line */
	SSM(sci, SCI_SETCARETLINEBACK, invert(common_style_set.styling[GCS_CURRENT_LINE].background), 0);
	/* bold=enable current line */
	SSM(sci, SCI_SETCARETLINEVISIBLE, common_style_set.styling[GCS_CURRENT_LINE].bold, 0);

	/* Translucency for current line and selection */
	SSM(sci, SCI_SETCARETLINEBACKALPHA, common_style_set.styling[GCS_TRANSLUCENCY].foreground, 0);
	SSM(sci, SCI_SETSELALPHA, common_style_set.styling[GCS_TRANSLUCENCY].background, 0);

	/* line wrapping visuals */
	SSM(sci, SCI_SETWRAPVISUALFLAGS,
		common_style_set.styling[GCS_LINE_WRAP_VISUALS].foreground, 0);
	SSM(sci, SCI_SETWRAPVISUALFLAGSLOCATION,
		common_style_set.styling[GCS_LINE_WRAP_VISUALS].background, 0);
	SSM(sci, SCI_SETWRAPSTARTINDENT, common_style_set.styling[GCS_LINE_WRAP_INDENT].foreground, 0);
	SSM(sci, SCI_SETWRAPINDENTMODE, common_style_set.styling[GCS_LINE_WRAP_INDENT].background, 0);

	/* Error indicator */
	SSM(sci, SCI_INDICSETSTYLE, GEANY_INDICATOR_ERROR, INDIC_SQUIGGLE);
	SSM(sci, SCI_INDICSETFORE, GEANY_INDICATOR_ERROR, invert(rotate_rgb(0xff0000)));

	/* Search indicator, used for 'Mark' matches */
	SSM(sci, SCI_INDICSETSTYLE, GEANY_INDICATOR_SEARCH, INDIC_ROUNDBOX);
	SSM(sci, SCI_INDICSETFORE, GEANY_INDICATOR_SEARCH,
		invert(common_style_set.styling[GCS_MARKER_SEARCH].background));
	SSM(sci, SCI_INDICSETALPHA, GEANY_INDICATOR_SEARCH, 60);

	/* define marker symbols
	 * 0 -> line marker */
	SSM(sci, SCI_MARKERDEFINE, 0, SC_MARK_SHORTARROW);
	SSM(sci, SCI_MARKERSETFORE, 0, invert(common_style_set.styling[GCS_MARKER_LINE].foreground));
	SSM(sci, SCI_MARKERSETBACK, 0, invert(common_style_set.styling[GCS_MARKER_LINE].background));
	SSM(sci, SCI_MARKERSETALPHA, 0, common_style_set.styling[GCS_MARKER_TRANSLUCENCY].foreground);

	/* 1 -> user marker */
	SSM(sci, SCI_MARKERDEFINE, 1, SC_MARK_PLUS);
	SSM(sci, SCI_MARKERSETFORE, 1, invert(common_style_set.styling[GCS_MARKER_MARK].foreground));
	SSM(sci, SCI_MARKERSETBACK, 1, invert(common_style_set.styling[GCS_MARKER_MARK].background));
	SSM(sci, SCI_MARKERSETALPHA, 1, common_style_set.styling[GCS_MARKER_TRANSLUCENCY].background);

	/* 2 -> folding marker, other folding settings */
	SSM(sci, SCI_SETMARGINTYPEN, 2, SC_MARGIN_SYMBOL);
	SSM(sci, SCI_SETMARGINMASKN, 2, SC_MASK_FOLDERS);

	/* drawing a horizontal line when text if folded */
	switch (common_style_set.fold_draw_line)
	{
		case 1:
		{
			SSM(sci, SCI_SETFOLDFLAGS, 4, 0);
			break;
		}
		case 2:
		{
			SSM(sci, SCI_SETFOLDFLAGS, 16, 0);
			break;
		}
		default:
		{
			SSM(sci, SCI_SETFOLDFLAGS, 0, 0);
			break;
		}
	}

	/* choose the folding style - boxes or circles, I prefer boxes, so it is default ;-) */
	switch (common_style_set.fold_marker)
	{
		case 2:
		{
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEROPEN, SC_MARK_CIRCLEMINUS);
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDER, SC_MARK_CIRCLEPLUS);
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEREND, SC_MARK_CIRCLEPLUSCONNECTED);
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEROPENMID, SC_MARK_CIRCLEMINUSCONNECTED);
			break;
		}
		default:
		{
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED);
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED);
			break;
		}
	}

	/* choose the folding style - straight or curved, I prefer straight, so it is default ;-) */
	switch (common_style_set.fold_lines)
	{
		case 2:
		{
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNERCURVE);
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNERCURVE);
			break;
		}
		default:
		{
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER);
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER);
			break;
		}
	}

	SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);

	SSM(sci, SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPEN, 0xffffff);
	SSM(sci, SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPEN, 0x000000);
	SSM(sci, SCI_MARKERSETFORE, SC_MARKNUM_FOLDER, 0xffffff);
	SSM(sci, SCI_MARKERSETBACK, SC_MARKNUM_FOLDER, 0x000000);
	SSM(sci, SCI_MARKERSETFORE, SC_MARKNUM_FOLDERSUB, 0xffffff);
	SSM(sci, SCI_MARKERSETBACK, SC_MARKNUM_FOLDERSUB, 0x000000);
	SSM(sci, SCI_MARKERSETFORE, SC_MARKNUM_FOLDERTAIL, 0xffffff);
	SSM(sci, SCI_MARKERSETBACK, SC_MARKNUM_FOLDERTAIL, 0x000000);
	SSM(sci, SCI_MARKERSETFORE, SC_MARKNUM_FOLDEREND, 0xffffff);
	SSM(sci, SCI_MARKERSETBACK, SC_MARKNUM_FOLDEREND, 0x000000);
	SSM(sci, SCI_MARKERSETFORE, SC_MARKNUM_FOLDEROPENMID, 0xffffff);
	SSM(sci, SCI_MARKERSETBACK, SC_MARKNUM_FOLDEROPENMID, 0x000000);
	SSM(sci, SCI_MARKERSETFORE, SC_MARKNUM_FOLDERMIDTAIL, 0xffffff);
	SSM(sci, SCI_MARKERSETBACK, SC_MARKNUM_FOLDERMIDTAIL, 0x000000);

	SSM(sci, SCI_SETPROPERTY, (uptr_t) "fold", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "fold.compact", (sptr_t) "0");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "fold.comment", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "fold.preprocessor", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "fold.at.else", (sptr_t) "1");


	/* bold (3rd argument) is whether to override default foreground selection */
	if (common_style_set.styling[GCS_SELECTION].bold)
		SSM(sci, SCI_SETSELFORE, 1, invert(common_style_set.styling[GCS_SELECTION].foreground));
	/* italic (4th argument) is whether to override default background selection */
	if (common_style_set.styling[GCS_SELECTION].italic)
		SSM(sci, SCI_SETSELBACK, 1, invert(common_style_set.styling[GCS_SELECTION].background));

	SSM(sci, SCI_SETSTYLEBITS, SSM(sci, SCI_GETSTYLEBITSNEEDED, 0, 0), 0);

	SSM(sci, SCI_SETFOLDMARGINCOLOUR, 1, invert(common_style_set.styling[GCS_MARGIN_FOLDING].background));
	/*SSM(sci, SCI_SETFOLDMARGINHICOLOUR, 1, invert(common_style_set.styling[GCS_MARGIN_FOLDING].background));*/
	set_sci_style(sci, STYLE_LINENUMBER, GEANY_FILETYPES_NONE, GCS_MARGIN_LINENUMBER);
	set_sci_style(sci, STYLE_BRACELIGHT, GEANY_FILETYPES_NONE, GCS_BRACE_GOOD);
	set_sci_style(sci, STYLE_BRACEBAD, GEANY_FILETYPES_NONE, GCS_BRACE_BAD);
	set_sci_style(sci, STYLE_INDENTGUIDE, GEANY_FILETYPES_NONE, GCS_INDENT_GUIDE);

	/* bold = common whitespace settings enabled */
	SSM(sci, SCI_SETWHITESPACEFORE, common_style_set.styling[GCS_WHITE_SPACE].bold,
		invert(common_style_set.styling[GCS_WHITE_SPACE].foreground));
	SSM(sci, SCI_SETWHITESPACEBACK, common_style_set.styling[GCS_WHITE_SPACE].italic,
		invert(common_style_set.styling[GCS_WHITE_SPACE].background));
}


/* Assign global typedefs and user secondary keywords */
static void assign_global_and_user_keywords(ScintillaObject *sci,
											const gchar *user_words, gint lang)
{
	GString *s;

	s = get_global_typenames(lang);
	if (G_UNLIKELY(s == NULL))
		s = g_string_sized_new(200);
	else
		g_string_append_c(s, ' '); /* append a space as delimiter to the existing list of words */

	g_string_append(s, user_words);

	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) s->str);
	g_string_free(s, TRUE);
}


/* All stylesets except None should call this. */
static void
apply_filetype_properties(ScintillaObject *sci, gint lexer, filetype_id ft_id)
{
	g_assert(ft_id != GEANY_FILETYPES_NONE);

	SSM(sci, SCI_SETLEXER, lexer, 0);

	styleset_common(sci, ft_id);
}


#define foreach_range(i, size) \
		for (i = 0; i < size; i++)

/* names: the style names for the filetype. */
static void load_style_entries(GKeyFile *config, GKeyFile *config_home, gint filetype_idx,
		const gchar **names, gsize names_len)
{
	guint i;

	foreach_range(i, names_len)
	{
		const gchar *name = names[i];
		GeanyLexerStyle *style = &style_sets[filetype_idx].styling[i];

		get_keyfile_style(config, config_home, name, style);
	}
}


/* styles: the style IDs for the filetype.
 * STYLE_DEFAULT will be set to match the first style. */
static void apply_style_entries(ScintillaObject *sci, gint filetype_idx,
		gint *styles, gsize styles_len)
{
	guint i;

	g_return_if_fail(styles_len > 0);

	set_sci_style(sci, STYLE_DEFAULT, filetype_idx, 0);

	foreach_range(i, styles_len)
		set_sci_style(sci, styles[i], filetype_idx, i);
}


/* call new_styleset(filetype_idx, >= 20) before using this. */
static void
styleset_c_like_init(GKeyFile *config, GKeyFile *config_home, gint filetype_idx)
{
	const gchar *entries[] =
 	{
		"default",
		"comment",
		"commentline",
		"commentdoc",
		"number",
		"word",
		"word2",
		"string",
		"character",
		"uuid",
		"preprocessor",
		"operator",
		"identifier",
		"stringeol",
		"verbatim",
		"regex",
		"commentlinedoc",
		"commentdockeyword",
		"commentdockeyworderror",
		"globalclass"
	};

	load_style_entries(config, config_home, filetype_idx, entries, G_N_ELEMENTS(entries));
}


static void styleset_c_like(ScintillaObject *sci, gint filetype_idx)
{
	gint styles[] = {
		SCE_C_DEFAULT,
		SCE_C_COMMENT,
		SCE_C_COMMENTLINE,
		SCE_C_COMMENTDOC,
		SCE_C_NUMBER,
		SCE_C_WORD,
		SCE_C_WORD2,
		SCE_C_STRING,
		SCE_C_CHARACTER,
		SCE_C_UUID,
		SCE_C_PREPROCESSOR,
		SCE_C_OPERATOR,
		SCE_C_IDENTIFIER,
		SCE_C_STRINGEOL,
		SCE_C_VERBATIM,
		SCE_C_REGEX,
		SCE_C_COMMENTLINEDOC,
		SCE_C_COMMENTDOCKEYWORD,
		SCE_C_COMMENTDOCKEYWORDERROR,
		/* used for local structs and typedefs */
		SCE_C_GLOBALCLASS
	};

	apply_style_entries(sci, filetype_idx, styles, G_N_ELEMENTS(styles));
}


static void styleset_c_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_C, 21);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_C);
	get_keyfile_int(config, config_home, "styling", "styling_within_preprocessor",
		1, 0, &style_sets[GEANY_FILETYPES_C].styling[20]);

	style_sets[GEANY_FILETYPES_C].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_C, 0);
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_C, 1);
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_C, 2);
	style_sets[GEANY_FILETYPES_C].keywords[3] = NULL;
}


static void styleset_c(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_C;

	apply_filetype_properties(sci, SCLEX_CPP, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_C].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_C].keywords[2]);

	/* assign global types, merge them with user defined keywords and set them */
	assign_global_and_user_keywords(sci, style_sets[GEANY_FILETYPES_C].keywords[1],
		filetypes[ft_id]->lang);

	styleset_c_like(sci, GEANY_FILETYPES_C);

	if (style_sets[GEANY_FILETYPES_C].styling[20].foreground == 1)
		SSM(sci, SCI_SETPROPERTY, (uptr_t) "styling.within.preprocessor", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "preprocessor.symbol.$(file.patterns.cpp)", (sptr_t) "#");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "preprocessor.start.$(file.patterns.cpp)", (sptr_t) "if ifdef ifndef");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "preprocessor.middle.$(file.patterns.cpp)", (sptr_t) "else elif");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "preprocessor.end.$(file.patterns.cpp)", (sptr_t) "endif");
}


static void styleset_cpp_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_CPP, 21);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_CPP);
	get_keyfile_int(config, config_home, "styling", "styling_within_preprocessor",
		1, 0, &style_sets[GEANY_FILETYPES_CPP].styling[20]);

	style_sets[GEANY_FILETYPES_CPP].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_CPP, 0);
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_CPP, 1);
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_CPP, 2);
	style_sets[GEANY_FILETYPES_CPP].keywords[3] = NULL;
}


static void styleset_cpp(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_CPP;

	apply_filetype_properties(sci, SCLEX_CPP, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_CPP].keywords[0]);
	/* for SCI_SETKEYWORDS = 1, see below*/
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_CPP].keywords[2]);

	/* assign global types, merge them with user defined keywords and set them */
	assign_global_and_user_keywords(sci, style_sets[GEANY_FILETYPES_CPP].keywords[1],
		filetypes[ft_id]->lang);

	styleset_c_like(sci, GEANY_FILETYPES_CPP);

	if (style_sets[GEANY_FILETYPES_CPP].styling[20].foreground == 1)
		SSM(sci, SCI_SETPROPERTY, (uptr_t) "styling.within.preprocessor", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "preprocessor.symbol.$(file.patterns.cpp)", (sptr_t) "#");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "preprocessor.start.$(file.patterns.cpp)", (sptr_t) "if ifdef ifndef");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "preprocessor.middle.$(file.patterns.cpp)", (sptr_t) "else elif");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "preprocessor.end.$(file.patterns.cpp)", (sptr_t) "endif");
}


static void styleset_glsl_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_GLSL, 21);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_GLSL);
	get_keyfile_int(config, config_home, "styling", "styling_within_preprocessor",
		1, 0, &style_sets[GEANY_FILETYPES_GLSL].styling[20]);

	style_sets[GEANY_FILETYPES_GLSL].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_GLSL, 0);
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_GLSL, 1);
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_GLSL, 2);
	style_sets[GEANY_FILETYPES_GLSL].keywords[3] = NULL;
}


static void styleset_glsl(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_GLSL;

	apply_filetype_properties(sci, SCLEX_CPP, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_GLSL].keywords[0]);
	/* for SCI_SETKEYWORDS = 1, see below*/
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_GLSL].keywords[2]);

	/* assign global types, merge them with user defined keywords and set them */
	assign_global_and_user_keywords(sci, style_sets[GEANY_FILETYPES_GLSL].keywords[1],
		filetypes[ft_id]->lang);

	styleset_c_like(sci, GEANY_FILETYPES_GLSL);

	if (style_sets[GEANY_FILETYPES_GLSL].styling[20].foreground == 1)
		SSM(sci, SCI_SETPROPERTY, (uptr_t) "styling.within.preprocessor", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "preprocessor.symbol.$(file.patterns.cpp)", (sptr_t) "#");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "preprocessor.start.$(file.patterns.cpp)", (sptr_t) "if ifdef ifndef");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "preprocessor.middle.$(file.patterns.cpp)", (sptr_t) "else elif");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "preprocessor.end.$(file.patterns.cpp)", (sptr_t) "endif");
}


static void styleset_cs_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_CS, 21);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_CS);
	get_keyfile_int(config, config_home, "styling", "styling_within_preprocessor",
		1, 0, &style_sets[GEANY_FILETYPES_CS].styling[20]);

	style_sets[GEANY_FILETYPES_CS].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_CS, 0);
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_CS, 1);
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_CS, 2);
	style_sets[GEANY_FILETYPES_CS].keywords[3] = NULL;
}


static void styleset_cs(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_CS;

	apply_filetype_properties(sci, SCLEX_CPP, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[ft_id].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[ft_id].keywords[2]);

	/* assign global types, merge them with user defined keywords and set them */
	assign_global_and_user_keywords(sci, style_sets[ft_id].keywords[1], filetypes[ft_id]->lang);

	styleset_c_like(sci, ft_id);

	if (style_sets[ft_id].styling[20].foreground == 1)
		SSM(sci, ft_id, (sptr_t) "styling.within.preprocessor", (sptr_t) "1");
}


static void styleset_vala_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_VALA, 21);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_VALA);
	get_keyfile_int(config, config_home, "styling", "styling_within_preprocessor",
		1, 0, &style_sets[GEANY_FILETYPES_VALA].styling[20]);

	style_sets[GEANY_FILETYPES_VALA].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_VALA, 0);
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_VALA, 1);
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_VALA, 2);
	style_sets[GEANY_FILETYPES_VALA].keywords[3] = NULL;
}


static void styleset_vala(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_VALA;

	apply_filetype_properties(sci, SCLEX_CPP, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[ft_id].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[ft_id].keywords[2]);

	/* assign global types, merge them with user defined keywords and set them */
	assign_global_and_user_keywords(sci, style_sets[ft_id].keywords[1], filetypes[ft_id]->lang);

	styleset_c_like(sci, ft_id);

	if (style_sets[ft_id].styling[20].foreground == 1)
		SSM(sci, ft_id, (sptr_t) "styling.within.preprocessor", (sptr_t) "1");
}


static void styleset_pascal_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_PASCAL, 15);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_PASCAL].styling[0]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_PASCAL].styling[1]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_PASCAL].styling[2]);
	get_keyfile_style(config, config_home, "comment2", &style_sets[GEANY_FILETYPES_PASCAL].styling[3]);
	get_keyfile_style(config, config_home, "commentline", &style_sets[GEANY_FILETYPES_PASCAL].styling[4]);
	get_keyfile_style(config, config_home, "preprocessor", &style_sets[GEANY_FILETYPES_PASCAL].styling[5]);
	get_keyfile_style(config, config_home, "preprocessor2", &style_sets[GEANY_FILETYPES_PASCAL].styling[6]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_PASCAL].styling[7]);
	get_keyfile_style(config, config_home, "hexnumber", &style_sets[GEANY_FILETYPES_PASCAL].styling[8]);
	get_keyfile_style(config, config_home, "word", &style_sets[GEANY_FILETYPES_PASCAL].styling[9]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_PASCAL].styling[10]);
	get_keyfile_style(config, config_home, "stringeol", &style_sets[GEANY_FILETYPES_PASCAL].styling[11]);
	get_keyfile_style(config, config_home, "character", &style_sets[GEANY_FILETYPES_PASCAL].styling[12]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_PASCAL].styling[13]);
	get_keyfile_style(config, config_home, "asm", &style_sets[GEANY_FILETYPES_PASCAL].styling[14]);

	style_sets[GEANY_FILETYPES_PASCAL].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_PASCAL, 0);
	style_sets[GEANY_FILETYPES_PASCAL].keywords[1] = NULL;
}


static void styleset_pascal(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_PASCAL;

	apply_filetype_properties(sci, SCLEX_PASCAL, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_PASCAL].keywords[0]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_PASCAL, 0);
	set_sci_style(sci, SCE_PAS_DEFAULT, GEANY_FILETYPES_PASCAL, 0);
	set_sci_style(sci, SCE_PAS_IDENTIFIER, GEANY_FILETYPES_PASCAL, 1);
	set_sci_style(sci, SCE_PAS_COMMENT, GEANY_FILETYPES_PASCAL, 2);
	set_sci_style(sci, SCE_PAS_COMMENT2, GEANY_FILETYPES_PASCAL, 3);
	set_sci_style(sci, SCE_PAS_COMMENTLINE, GEANY_FILETYPES_PASCAL, 4);
	set_sci_style(sci, SCE_PAS_PREPROCESSOR, GEANY_FILETYPES_PASCAL, 5);
	set_sci_style(sci, SCE_PAS_PREPROCESSOR2, GEANY_FILETYPES_PASCAL, 6);
	set_sci_style(sci, SCE_PAS_NUMBER, GEANY_FILETYPES_PASCAL, 7);
	set_sci_style(sci, SCE_PAS_HEXNUMBER, GEANY_FILETYPES_PASCAL, 8);
	set_sci_style(sci, SCE_PAS_WORD, GEANY_FILETYPES_PASCAL, 9);
	set_sci_style(sci, SCE_PAS_STRING, GEANY_FILETYPES_PASCAL, 10);
	set_sci_style(sci, SCE_PAS_STRINGEOL, GEANY_FILETYPES_PASCAL, 11);
	set_sci_style(sci, SCE_PAS_CHARACTER, GEANY_FILETYPES_PASCAL, 12);
	set_sci_style(sci, SCE_PAS_OPERATOR, GEANY_FILETYPES_PASCAL, 13);
	set_sci_style(sci, SCE_PAS_ASM, GEANY_FILETYPES_PASCAL, 14);

	SSM(sci, SCI_SETPROPERTY, (uptr_t) "lexer.pascal.smart.highlighting", (sptr_t) "1");
}


static void styleset_makefile_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_MAKE, 7);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_MAKE].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_MAKE].styling[1]);
	get_keyfile_style(config, config_home, "preprocessor", &style_sets[GEANY_FILETYPES_MAKE].styling[2]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_MAKE].styling[3]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_MAKE].styling[4]);
	get_keyfile_style(config, config_home, "target", &style_sets[GEANY_FILETYPES_MAKE].styling[5]);
	get_keyfile_style(config, config_home, "ideol", &style_sets[GEANY_FILETYPES_MAKE].styling[6]);

	style_sets[GEANY_FILETYPES_MAKE].keywords = NULL;
}


static void styleset_makefile(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_MAKE;

	apply_filetype_properties(sci, SCLEX_MAKEFILE, ft_id);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_MAKE, 0);
	set_sci_style(sci, SCE_MAKE_DEFAULT, GEANY_FILETYPES_MAKE, 0);
	set_sci_style(sci, SCE_MAKE_COMMENT, GEANY_FILETYPES_MAKE, 1);
	set_sci_style(sci, SCE_MAKE_PREPROCESSOR, GEANY_FILETYPES_MAKE, 2);
	set_sci_style(sci, SCE_MAKE_IDENTIFIER, GEANY_FILETYPES_MAKE, 3);
	set_sci_style(sci, SCE_MAKE_OPERATOR, GEANY_FILETYPES_MAKE, 4);
	set_sci_style(sci, SCE_MAKE_TARGET, GEANY_FILETYPES_MAKE, 5);
	set_sci_style(sci, SCE_MAKE_IDEOL, GEANY_FILETYPES_MAKE, 6);
}


static void styleset_diff_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_DIFF, 8);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_DIFF].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_DIFF].styling[1]);
	get_keyfile_style(config, config_home, "command", &style_sets[GEANY_FILETYPES_DIFF].styling[2]);
	get_keyfile_style(config, config_home, "header", &style_sets[GEANY_FILETYPES_DIFF].styling[3]);
	get_keyfile_style(config, config_home, "position", &style_sets[GEANY_FILETYPES_DIFF].styling[4]);
	get_keyfile_style(config, config_home, "deleted", &style_sets[GEANY_FILETYPES_DIFF].styling[5]);
	get_keyfile_style(config, config_home, "added", &style_sets[GEANY_FILETYPES_DIFF].styling[6]);
	get_keyfile_style(config, config_home, "changed", &style_sets[GEANY_FILETYPES_DIFF].styling[7]);

	style_sets[GEANY_FILETYPES_DIFF].keywords = NULL;
}


static void styleset_diff(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_DIFF;

	apply_filetype_properties(sci, SCLEX_DIFF, ft_id);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_DIFF, 0);
	set_sci_style(sci, SCE_DIFF_DEFAULT, GEANY_FILETYPES_DIFF, 0);
	set_sci_style(sci, SCE_DIFF_COMMENT, GEANY_FILETYPES_DIFF, 1);
	set_sci_style(sci, SCE_DIFF_COMMAND, GEANY_FILETYPES_DIFF, 2);
	set_sci_style(sci, SCE_DIFF_HEADER, GEANY_FILETYPES_DIFF, 3);
	set_sci_style(sci, SCE_DIFF_POSITION, GEANY_FILETYPES_DIFF, 4);
	set_sci_style(sci, SCE_DIFF_DELETED, GEANY_FILETYPES_DIFF, 5);
	set_sci_style(sci, SCE_DIFF_ADDED, GEANY_FILETYPES_DIFF, 6);
	set_sci_style(sci, SCE_DIFF_CHANGED, GEANY_FILETYPES_DIFF, 7);
}


static void styleset_latex_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_LATEX, 5);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_LATEX].styling[0]);
	get_keyfile_style(config, config_home, "command", &style_sets[GEANY_FILETYPES_LATEX].styling[1]);
	get_keyfile_style(config, config_home, "tag", &style_sets[GEANY_FILETYPES_LATEX].styling[2]);
	get_keyfile_style(config, config_home, "math", &style_sets[GEANY_FILETYPES_LATEX].styling[3]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_LATEX].styling[4]);

	style_sets[GEANY_FILETYPES_LATEX].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_LATEX, 0);
	style_sets[GEANY_FILETYPES_LATEX].keywords[1] = NULL;
}


static void styleset_latex(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_LATEX;

	apply_filetype_properties(sci, SCLEX_LATEX, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_LATEX].keywords[0]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_LATEX, 0);
	set_sci_style(sci, SCE_L_DEFAULT, GEANY_FILETYPES_LATEX, 0);
	set_sci_style(sci, SCE_L_COMMAND, GEANY_FILETYPES_LATEX, 1);
	set_sci_style(sci, SCE_L_TAG, GEANY_FILETYPES_LATEX, 2);
	set_sci_style(sci, SCE_L_MATH, GEANY_FILETYPES_LATEX, 3);
	set_sci_style(sci, SCE_L_COMMENT, GEANY_FILETYPES_LATEX, 4);
}


static void styleset_php_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	style_sets[GEANY_FILETYPES_PHP].styling = NULL;
	style_sets[GEANY_FILETYPES_PHP].keywords = NULL;
}


static void styleset_php(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_PHP;

	apply_filetype_properties(sci, SCLEX_HTML, ft_id);

	SSM(sci, SCI_SETPROPERTY, (uptr_t) "phpscript.mode", (sptr_t) "1");

	/* use the same colouring as for XML */
	styleset_markup(sci, TRUE);
}


static void styleset_html_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	style_sets[GEANY_FILETYPES_HTML].styling = NULL;
	style_sets[GEANY_FILETYPES_HTML].keywords = NULL;
}


static void styleset_html(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_HTML;

	apply_filetype_properties(sci, SCLEX_HTML, ft_id);

	/* use the same colouring for HTML; XML and so on */
	styleset_markup(sci, TRUE);
}


static void styleset_markup_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_XML, 57);
	get_keyfile_style(config, config_home, "html_default", &style_sets[GEANY_FILETYPES_XML].styling[0]);
	get_keyfile_style(config, config_home, "html_tag", &style_sets[GEANY_FILETYPES_XML].styling[1]);
	get_keyfile_style(config, config_home, "html_tagunknown", &style_sets[GEANY_FILETYPES_XML].styling[2]);
	get_keyfile_style(config, config_home, "html_attribute", &style_sets[GEANY_FILETYPES_XML].styling[3]);
	get_keyfile_style(config, config_home, "html_attributeunknown", &style_sets[GEANY_FILETYPES_XML].styling[4]);
	get_keyfile_style(config, config_home, "html_number", &style_sets[GEANY_FILETYPES_XML].styling[5]);
	get_keyfile_style(config, config_home, "html_doublestring", &style_sets[GEANY_FILETYPES_XML].styling[6]);
	get_keyfile_style(config, config_home, "html_singlestring", &style_sets[GEANY_FILETYPES_XML].styling[7]);
	get_keyfile_style(config, config_home, "html_other", &style_sets[GEANY_FILETYPES_XML].styling[8]);
	get_keyfile_style(config, config_home, "html_comment", &style_sets[GEANY_FILETYPES_XML].styling[9]);
	get_keyfile_style(config, config_home, "html_entity", &style_sets[GEANY_FILETYPES_XML].styling[10]);
	get_keyfile_style(config, config_home, "html_tagend", &style_sets[GEANY_FILETYPES_XML].styling[11]);
	get_keyfile_style(config, config_home, "html_xmlstart", &style_sets[GEANY_FILETYPES_XML].styling[12]);
	get_keyfile_style(config, config_home, "html_xmlend", &style_sets[GEANY_FILETYPES_XML].styling[13]);
	get_keyfile_style(config, config_home, "html_script", &style_sets[GEANY_FILETYPES_XML].styling[14]);
	get_keyfile_style(config, config_home, "html_asp", &style_sets[GEANY_FILETYPES_XML].styling[15]);
	get_keyfile_style(config, config_home, "html_aspat", &style_sets[GEANY_FILETYPES_XML].styling[16]);
	get_keyfile_style(config, config_home, "html_cdata", &style_sets[GEANY_FILETYPES_XML].styling[17]);
	get_keyfile_style(config, config_home, "html_question", &style_sets[GEANY_FILETYPES_XML].styling[18]);
	get_keyfile_style(config, config_home, "html_value", &style_sets[GEANY_FILETYPES_XML].styling[19]);
	get_keyfile_style(config, config_home, "html_xccomment", &style_sets[GEANY_FILETYPES_XML].styling[20]);

	get_keyfile_style(config, config_home, "sgml_default", &style_sets[GEANY_FILETYPES_XML].styling[21]);
	get_keyfile_style(config, config_home, "sgml_comment", &style_sets[GEANY_FILETYPES_XML].styling[22]);
	get_keyfile_style(config, config_home, "sgml_special", &style_sets[GEANY_FILETYPES_XML].styling[23]);
	get_keyfile_style(config, config_home, "sgml_command", &style_sets[GEANY_FILETYPES_XML].styling[24]);
	get_keyfile_style(config, config_home, "sgml_doublestring", &style_sets[GEANY_FILETYPES_XML].styling[25]);
	get_keyfile_style(config, config_home, "sgml_simplestring", &style_sets[GEANY_FILETYPES_XML].styling[26]);
	get_keyfile_style(config, config_home, "sgml_1st_param", &style_sets[GEANY_FILETYPES_XML].styling[27]);
	get_keyfile_style(config, config_home, "sgml_entity", &style_sets[GEANY_FILETYPES_XML].styling[28]);
	get_keyfile_style(config, config_home, "sgml_block_default", &style_sets[GEANY_FILETYPES_XML].styling[29]);
	get_keyfile_style(config, config_home, "sgml_1st_param_comment", &style_sets[GEANY_FILETYPES_XML].styling[30]);
	get_keyfile_style(config, config_home, "sgml_error", &style_sets[GEANY_FILETYPES_XML].styling[31]);

	get_keyfile_style(config, config_home, "php_default", &style_sets[GEANY_FILETYPES_XML].styling[32]);
	get_keyfile_style(config, config_home, "php_simplestring", &style_sets[GEANY_FILETYPES_XML].styling[33]);
	get_keyfile_style(config, config_home, "php_hstring", &style_sets[GEANY_FILETYPES_XML].styling[34]);
	get_keyfile_style(config, config_home, "php_number", &style_sets[GEANY_FILETYPES_XML].styling[35]);
	get_keyfile_style(config, config_home, "php_word", &style_sets[GEANY_FILETYPES_XML].styling[36]);
	get_keyfile_style(config, config_home, "php_variable", &style_sets[GEANY_FILETYPES_XML].styling[37]);
	get_keyfile_style(config, config_home, "php_comment", &style_sets[GEANY_FILETYPES_XML].styling[38]);
	get_keyfile_style(config, config_home, "php_commentline", &style_sets[GEANY_FILETYPES_XML].styling[39]);
	get_keyfile_style(config, config_home, "php_operator", &style_sets[GEANY_FILETYPES_XML].styling[40]);
	get_keyfile_style(config, config_home, "php_hstring_variable", &style_sets[GEANY_FILETYPES_XML].styling[41]);
	get_keyfile_style(config, config_home, "php_complex_variable", &style_sets[GEANY_FILETYPES_XML].styling[42]);

	get_keyfile_style(config, config_home, "jscript_start", &style_sets[GEANY_FILETYPES_XML].styling[43]);
	get_keyfile_style(config, config_home, "jscript_default", &style_sets[GEANY_FILETYPES_XML].styling[44]);
	get_keyfile_style(config, config_home, "jscript_comment", &style_sets[GEANY_FILETYPES_XML].styling[45]);
	get_keyfile_style(config, config_home, "jscript_commentline", &style_sets[GEANY_FILETYPES_XML].styling[46]);
	get_keyfile_style(config, config_home, "jscript_commentdoc", &style_sets[GEANY_FILETYPES_XML].styling[47]);
	get_keyfile_style(config, config_home, "jscript_number", &style_sets[GEANY_FILETYPES_XML].styling[48]);
	get_keyfile_style(config, config_home, "jscript_word", &style_sets[GEANY_FILETYPES_XML].styling[49]);
	get_keyfile_style(config, config_home, "jscript_keyword", &style_sets[GEANY_FILETYPES_XML].styling[50]);
	get_keyfile_style(config, config_home, "jscript_doublestring", &style_sets[GEANY_FILETYPES_XML].styling[51]);
	get_keyfile_style(config, config_home, "jscript_singlestring", &style_sets[GEANY_FILETYPES_XML].styling[52]);
	get_keyfile_style(config, config_home, "jscript_symbols", &style_sets[GEANY_FILETYPES_XML].styling[53]);
	get_keyfile_style(config, config_home, "jscript_stringeol", &style_sets[GEANY_FILETYPES_XML].styling[54]);
	get_keyfile_style(config, config_home, "jscript_regex", &style_sets[GEANY_FILETYPES_XML].styling[55]);

	get_keyfile_int(config, config_home, "styling", "html_asp_default_language", 1, 0, &style_sets[GEANY_FILETYPES_XML].styling[56]);

	style_sets[GEANY_FILETYPES_XML].keywords = g_new(gchar*, 7);
	get_keyfile_keywords(config, config_home, "html", GEANY_FILETYPES_XML, 0);
	get_keyfile_keywords(config, config_home, "javascript", GEANY_FILETYPES_XML, 1);
	get_keyfile_keywords(config, config_home, "vbscript", GEANY_FILETYPES_XML, 2);
	get_keyfile_keywords(config, config_home, "python", GEANY_FILETYPES_XML, 3);
	get_keyfile_keywords(config, config_home, "php", GEANY_FILETYPES_XML, 4);
	get_keyfile_keywords(config, config_home, "sgml", GEANY_FILETYPES_XML, 5);
	style_sets[GEANY_FILETYPES_XML].keywords[6] = NULL;
}


static void styleset_markup(ScintillaObject *sci, gboolean set_keywords)
{
	guint i;
	const gchar *keywords;

	/* Used by several filetypes */
	if (style_sets[GEANY_FILETYPES_XML].styling == NULL)
		filetypes_load_config(GEANY_FILETYPES_XML, FALSE);

	/* manually initialise filetype Python for use with embedded Python */
	filetypes_load_config(GEANY_FILETYPES_PYTHON, FALSE);

	/* Set keywords. If we don't want to use keywords, we must at least unset maybe previously set
	 * keywords, e.g. when switching between filetypes. */
	for (i = 0; i < 5; i++)
	{
		keywords = (set_keywords) ? style_sets[GEANY_FILETYPES_XML].keywords[i] : "";
		SSM(sci, SCI_SETKEYWORDS, i, (sptr_t) keywords);
	}
	SSM(sci, SCI_SETKEYWORDS, 5, (sptr_t) style_sets[GEANY_FILETYPES_XML].keywords[5]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_XML, 0);
	set_sci_style(sci, SCE_H_DEFAULT, GEANY_FILETYPES_XML, 0);
	set_sci_style(sci, SCE_H_TAG, GEANY_FILETYPES_XML, 1);
	set_sci_style(sci, SCE_H_TAGUNKNOWN, GEANY_FILETYPES_XML, 2);
	set_sci_style(sci, SCE_H_ATTRIBUTE, GEANY_FILETYPES_XML, 3);
	set_sci_style(sci, SCE_H_ATTRIBUTEUNKNOWN, GEANY_FILETYPES_XML, 4);
	set_sci_style(sci, SCE_H_NUMBER, GEANY_FILETYPES_XML, 5);
	set_sci_style(sci, SCE_H_DOUBLESTRING, GEANY_FILETYPES_XML, 6);
	set_sci_style(sci, SCE_H_SINGLESTRING, GEANY_FILETYPES_XML, 7);
	set_sci_style(sci, SCE_H_OTHER, GEANY_FILETYPES_XML, 8);
	set_sci_style(sci, SCE_H_COMMENT, GEANY_FILETYPES_XML, 9);
	set_sci_style(sci, SCE_H_ENTITY, GEANY_FILETYPES_XML, 10);
	set_sci_style(sci, SCE_H_TAGEND, GEANY_FILETYPES_XML, 11);
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_H_XMLSTART, 1);
	set_sci_style(sci, SCE_H_XMLSTART, GEANY_FILETYPES_XML, 12);
	set_sci_style(sci, SCE_H_XMLEND, GEANY_FILETYPES_XML, 13);
	set_sci_style(sci, SCE_H_SCRIPT, GEANY_FILETYPES_XML, 14);
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_H_ASP, 1);
	set_sci_style(sci, SCE_H_ASP, GEANY_FILETYPES_XML, 15);
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_H_ASPAT, 1);
	set_sci_style(sci, SCE_H_ASPAT, GEANY_FILETYPES_XML, 16);
	set_sci_style(sci, SCE_H_CDATA, GEANY_FILETYPES_XML, 17);
	set_sci_style(sci, SCE_H_QUESTION, GEANY_FILETYPES_XML, 18);
	set_sci_style(sci, SCE_H_VALUE, GEANY_FILETYPES_XML, 19);
	set_sci_style(sci, SCE_H_XCCOMMENT, GEANY_FILETYPES_XML, 20);

	set_sci_style(sci, SCE_H_SGML_DEFAULT, GEANY_FILETYPES_XML, 21);
	set_sci_style(sci, SCE_H_SGML_COMMENT, GEANY_FILETYPES_XML, 22);
	set_sci_style(sci, SCE_H_SGML_SPECIAL, GEANY_FILETYPES_XML, 23);
	set_sci_style(sci, SCE_H_SGML_COMMAND, GEANY_FILETYPES_XML, 24);
	set_sci_style(sci, SCE_H_SGML_DOUBLESTRING, GEANY_FILETYPES_XML, 25);
	set_sci_style(sci, SCE_H_SGML_SIMPLESTRING, GEANY_FILETYPES_XML, 26);
	set_sci_style(sci, SCE_H_SGML_1ST_PARAM, GEANY_FILETYPES_XML, 27);
	set_sci_style(sci, SCE_H_SGML_ENTITY, GEANY_FILETYPES_XML, 28);
	set_sci_style(sci, SCE_H_SGML_BLOCK_DEFAULT, GEANY_FILETYPES_XML, 29);
	set_sci_style(sci, SCE_H_SGML_1ST_PARAM_COMMENT, GEANY_FILETYPES_XML, 30);
	set_sci_style(sci, SCE_H_SGML_ERROR, GEANY_FILETYPES_XML, 31);

	/* embedded JavaScript */
	set_sci_style(sci, SCE_HJ_START, GEANY_FILETYPES_XML, 43);
	set_sci_style(sci, SCE_HJ_DEFAULT, GEANY_FILETYPES_XML, 44);
	set_sci_style(sci, SCE_HJ_COMMENT, GEANY_FILETYPES_XML, 45);
	set_sci_style(sci, SCE_HJ_COMMENTLINE, GEANY_FILETYPES_XML, 46);
	set_sci_style(sci, SCE_HJ_COMMENTDOC, GEANY_FILETYPES_XML, 47);
	set_sci_style(sci, SCE_HJ_NUMBER, GEANY_FILETYPES_XML, 48);
	set_sci_style(sci, SCE_HJ_WORD, GEANY_FILETYPES_XML, 49);
	set_sci_style(sci, SCE_HJ_KEYWORD, GEANY_FILETYPES_XML, 50);
	set_sci_style(sci, SCE_HJ_DOUBLESTRING, GEANY_FILETYPES_XML, 51);
	set_sci_style(sci, SCE_HJ_SINGLESTRING, GEANY_FILETYPES_XML, 52);
	set_sci_style(sci, SCE_HJ_SYMBOLS, GEANY_FILETYPES_XML, 53);
	set_sci_style(sci, SCE_HJ_STRINGEOL, GEANY_FILETYPES_XML, 54);
	set_sci_style(sci, SCE_HJ_REGEX, GEANY_FILETYPES_XML, 55);

	/* for HB, VBScript?, use the same styles as for JavaScript */
	set_sci_style(sci, SCE_HB_START, GEANY_FILETYPES_XML, 43);
	set_sci_style(sci, SCE_HB_DEFAULT, GEANY_FILETYPES_XML, 44);
	set_sci_style(sci, SCE_HB_COMMENTLINE, GEANY_FILETYPES_XML, 46);
	set_sci_style(sci, SCE_HB_NUMBER, GEANY_FILETYPES_XML, 48);
	set_sci_style(sci, SCE_HB_WORD, GEANY_FILETYPES_XML, 49);
	set_sci_style(sci, SCE_HB_STRING, GEANY_FILETYPES_XML, 51);
	set_sci_style(sci, SCE_HB_IDENTIFIER, GEANY_FILETYPES_XML, 53);
	set_sci_style(sci, SCE_HB_STRINGEOL, GEANY_FILETYPES_XML, 54);

	/* for HBA, VBScript?, use the same styles as for JavaScript */
	set_sci_style(sci, SCE_HBA_START, GEANY_FILETYPES_XML, 43);
	set_sci_style(sci, SCE_HBA_DEFAULT, GEANY_FILETYPES_XML, 44);
	set_sci_style(sci, SCE_HBA_COMMENTLINE, GEANY_FILETYPES_XML, 46);
	set_sci_style(sci, SCE_HBA_NUMBER, GEANY_FILETYPES_XML, 48);
	set_sci_style(sci, SCE_HBA_WORD, GEANY_FILETYPES_XML, 49);
	set_sci_style(sci, SCE_HBA_STRING, GEANY_FILETYPES_XML, 51);
	set_sci_style(sci, SCE_HBA_IDENTIFIER, GEANY_FILETYPES_XML, 53);
	set_sci_style(sci, SCE_HBA_STRINGEOL, GEANY_FILETYPES_XML, 54);

	/* for HJA, ASP Javascript, use the same styles as for JavaScript */
	set_sci_style(sci, SCE_HJA_START, GEANY_FILETYPES_XML, 43);
	set_sci_style(sci, SCE_HJA_DEFAULT, GEANY_FILETYPES_XML, 44);
	set_sci_style(sci, SCE_HJA_COMMENT, GEANY_FILETYPES_XML, 45);
	set_sci_style(sci, SCE_HJA_COMMENTLINE, GEANY_FILETYPES_XML, 46);
	set_sci_style(sci, SCE_HJA_COMMENTDOC, GEANY_FILETYPES_XML, 47);
	set_sci_style(sci, SCE_HJA_NUMBER, GEANY_FILETYPES_XML, 48);
	set_sci_style(sci, SCE_HJA_WORD, GEANY_FILETYPES_XML, 49);
	set_sci_style(sci, SCE_HJA_KEYWORD, GEANY_FILETYPES_XML, 50);
	set_sci_style(sci, SCE_HJA_DOUBLESTRING, GEANY_FILETYPES_XML, 51);
	set_sci_style(sci, SCE_HJA_SINGLESTRING, GEANY_FILETYPES_XML, 52);
	set_sci_style(sci, SCE_HJA_SYMBOLS, GEANY_FILETYPES_XML, 53);
	set_sci_style(sci, SCE_HJA_STRINGEOL, GEANY_FILETYPES_XML, 54);

	/* for embedded Python we use the Python styles */
	set_sci_style(sci, SCE_HP_START, GEANY_FILETYPES_XML, 43);
	set_sci_style(sci, SCE_HP_DEFAULT, GEANY_FILETYPES_PYTHON, 0);
	set_sci_style(sci, SCE_HP_COMMENTLINE, GEANY_FILETYPES_PYTHON, 1);
	set_sci_style(sci, SCE_HP_NUMBER, GEANY_FILETYPES_PYTHON, 2);
	set_sci_style(sci, SCE_HP_STRING, GEANY_FILETYPES_PYTHON, 3);
	set_sci_style(sci, SCE_HP_CHARACTER, GEANY_FILETYPES_PYTHON, 4);
	set_sci_style(sci, SCE_HP_WORD, GEANY_FILETYPES_PYTHON, 5);
	set_sci_style(sci, SCE_HP_TRIPLE, GEANY_FILETYPES_PYTHON, 6);
	set_sci_style(sci, SCE_HP_TRIPLEDOUBLE, GEANY_FILETYPES_PYTHON, 7);
	set_sci_style(sci, SCE_HP_CLASSNAME, GEANY_FILETYPES_PYTHON, 8);
	set_sci_style(sci, SCE_HP_DEFNAME, GEANY_FILETYPES_PYTHON, 9);
	set_sci_style(sci, SCE_HP_OPERATOR, GEANY_FILETYPES_PYTHON, 10);
	set_sci_style(sci, SCE_HP_IDENTIFIER, GEANY_FILETYPES_PYTHON, 11);

	/* for embedded HPA (what is this?) we use the Python styles */
	set_sci_style(sci, SCE_HPA_START, GEANY_FILETYPES_XML, 43);
	set_sci_style(sci, SCE_HPA_DEFAULT, GEANY_FILETYPES_PYTHON, 0);
	set_sci_style(sci, SCE_HPA_COMMENTLINE, GEANY_FILETYPES_PYTHON, 1);
	set_sci_style(sci, SCE_HPA_NUMBER, GEANY_FILETYPES_PYTHON, 2);
	set_sci_style(sci, SCE_HPA_STRING, GEANY_FILETYPES_PYTHON, 3);
	set_sci_style(sci, SCE_HPA_CHARACTER, GEANY_FILETYPES_PYTHON, 4);
	set_sci_style(sci, SCE_HPA_WORD, GEANY_FILETYPES_PYTHON, 5);
	set_sci_style(sci, SCE_HPA_TRIPLE, GEANY_FILETYPES_PYTHON, 6);
	set_sci_style(sci, SCE_HPA_TRIPLEDOUBLE, GEANY_FILETYPES_PYTHON, 7);
	set_sci_style(sci, SCE_HPA_CLASSNAME, GEANY_FILETYPES_PYTHON, 8);
	set_sci_style(sci, SCE_HPA_DEFNAME, GEANY_FILETYPES_PYTHON, 9);
	set_sci_style(sci, SCE_HPA_OPERATOR, GEANY_FILETYPES_PYTHON, 10);
	set_sci_style(sci, SCE_HPA_IDENTIFIER, GEANY_FILETYPES_PYTHON, 11);

	/* PHP */
	set_sci_style(sci, SCE_HPHP_DEFAULT, GEANY_FILETYPES_XML, 32);
	set_sci_style(sci, SCE_HPHP_SIMPLESTRING, GEANY_FILETYPES_XML, 33);
	set_sci_style(sci, SCE_HPHP_HSTRING, GEANY_FILETYPES_XML, 34);
	set_sci_style(sci, SCE_HPHP_NUMBER, GEANY_FILETYPES_XML, 35);
	set_sci_style(sci, SCE_HPHP_WORD, GEANY_FILETYPES_XML, 36);
	set_sci_style(sci, SCE_HPHP_VARIABLE, GEANY_FILETYPES_XML, 37);
	set_sci_style(sci, SCE_HPHP_COMMENT, GEANY_FILETYPES_XML, 38);
	set_sci_style(sci, SCE_HPHP_COMMENTLINE, GEANY_FILETYPES_XML, 39);
	set_sci_style(sci, SCE_HPHP_OPERATOR, GEANY_FILETYPES_XML, 40);
	set_sci_style(sci, SCE_HPHP_HSTRING_VARIABLE, GEANY_FILETYPES_XML, 41);
	set_sci_style(sci, SCE_HPHP_COMPLEX_VARIABLE, GEANY_FILETYPES_XML, 42);

	SSM(sci, SCI_SETPROPERTY, (uptr_t) "fold.html", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "fold.html.preprocessor", (sptr_t) "0");

	{
		gint asp_default_language;
		gchar *str;

		asp_default_language = style_sets[GEANY_FILETYPES_XML].styling[56].foreground;
		str = g_strdup_printf("%d", asp_default_language);
		SSM(sci, SCI_SETPROPERTY, (uptr_t) "asp.default.language", (sptr_t) &str[0]);
		g_free(str);
	}
}


static void styleset_java_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_JAVA, 20);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_JAVA);

	style_sets[GEANY_FILETYPES_JAVA].keywords = g_new(gchar*, 5);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_JAVA, 0);
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_JAVA, 1);
	get_keyfile_keywords(config, config_home, "doccomment", GEANY_FILETYPES_JAVA, 2);
	get_keyfile_keywords(config, config_home, "typedefs", GEANY_FILETYPES_JAVA, 3);
	style_sets[GEANY_FILETYPES_JAVA].keywords[4] = NULL;
}


static void styleset_java(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_JAVA;

	apply_filetype_properties(sci, SCLEX_CPP, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_JAVA].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_JAVA].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_JAVA].keywords[2]);
	SSM(sci, SCI_SETKEYWORDS, 4, (sptr_t) style_sets[GEANY_FILETYPES_JAVA].keywords[3]);

	styleset_c_like(sci, GEANY_FILETYPES_JAVA);
}


static void styleset_perl_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_PERL, 35);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_PERL].styling[0]);
	get_keyfile_style(config, config_home, "error", &style_sets[GEANY_FILETYPES_PERL].styling[1]);
	get_keyfile_style(config, config_home, "commentline", &style_sets[GEANY_FILETYPES_PERL].styling[2]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_PERL].styling[3]);
	get_keyfile_style(config, config_home, "word", &style_sets[GEANY_FILETYPES_PERL].styling[4]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_PERL].styling[5]);
	get_keyfile_style(config, config_home, "character", &style_sets[GEANY_FILETYPES_PERL].styling[6]);
	get_keyfile_style(config, config_home, "preprocessor", &style_sets[GEANY_FILETYPES_PERL].styling[7]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_PERL].styling[8]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_PERL].styling[9]);
	get_keyfile_style(config, config_home, "scalar", &style_sets[GEANY_FILETYPES_PERL].styling[10]);
	get_keyfile_style(config, config_home, "pod", &style_sets[GEANY_FILETYPES_PERL].styling[11]);
	get_keyfile_style(config, config_home, "regex", &style_sets[GEANY_FILETYPES_PERL].styling[12]);
	get_keyfile_style(config, config_home, "array", &style_sets[GEANY_FILETYPES_PERL].styling[13]);
	get_keyfile_style(config, config_home, "hash", &style_sets[GEANY_FILETYPES_PERL].styling[14]);
	get_keyfile_style(config, config_home, "symboltable", &style_sets[GEANY_FILETYPES_PERL].styling[15]);
	get_keyfile_style(config, config_home, "backticks", &style_sets[GEANY_FILETYPES_PERL].styling[16]);
	get_keyfile_style(config, config_home, "pod_verbatim", &style_sets[GEANY_FILETYPES_PERL].styling[17]);
	get_keyfile_style(config, config_home, "reg_subst", &style_sets[GEANY_FILETYPES_PERL].styling[18]);
	get_keyfile_style(config, config_home, "datasection", &style_sets[GEANY_FILETYPES_PERL].styling[19]);
	get_keyfile_style(config, config_home, "here_delim", &style_sets[GEANY_FILETYPES_PERL].styling[20]);
	get_keyfile_style(config, config_home, "here_q", &style_sets[GEANY_FILETYPES_PERL].styling[21]);
	get_keyfile_style(config, config_home, "here_qq", &style_sets[GEANY_FILETYPES_PERL].styling[22]);
	get_keyfile_style(config, config_home, "here_qx", &style_sets[GEANY_FILETYPES_PERL].styling[23]);
	get_keyfile_style(config, config_home, "string_q", &style_sets[GEANY_FILETYPES_PERL].styling[24]);
	get_keyfile_style(config, config_home, "string_qq", &style_sets[GEANY_FILETYPES_PERL].styling[25]);
	get_keyfile_style(config, config_home, "string_qx", &style_sets[GEANY_FILETYPES_PERL].styling[26]);
	get_keyfile_style(config, config_home, "string_qr", &style_sets[GEANY_FILETYPES_PERL].styling[27]);
	get_keyfile_style(config, config_home, "string_qw", &style_sets[GEANY_FILETYPES_PERL].styling[28]);
	get_keyfile_style(config, config_home, "variable_indexer", &style_sets[GEANY_FILETYPES_PERL].styling[29]);
	get_keyfile_style(config, config_home, "punctuation", &style_sets[GEANY_FILETYPES_PERL].styling[30]);
	get_keyfile_style(config, config_home, "longquote", &style_sets[GEANY_FILETYPES_PERL].styling[31]);
	get_keyfile_style(config, config_home, "sub_prototype", &style_sets[GEANY_FILETYPES_PERL].styling[32]);
	get_keyfile_style(config, config_home, "format_ident", &style_sets[GEANY_FILETYPES_PERL].styling[33]);
	get_keyfile_style(config, config_home, "format", &style_sets[GEANY_FILETYPES_PERL].styling[34]);


	style_sets[GEANY_FILETYPES_PERL].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_PERL, 0);
	style_sets[GEANY_FILETYPES_PERL].keywords[1] = NULL;
}


static void styleset_perl(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_PERL;

	apply_filetype_properties(sci, SCLEX_PERL, ft_id);

	SSM(sci, SCI_SETPROPERTY, (uptr_t) "styling.within.preprocessor", (sptr_t) "1");

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_PERL].keywords[0]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_PERL, 0);
	set_sci_style(sci, SCE_PL_DEFAULT, GEANY_FILETYPES_PERL, 0);
	set_sci_style(sci, SCE_PL_ERROR, GEANY_FILETYPES_PERL, 1);
	set_sci_style(sci, SCE_PL_COMMENTLINE, GEANY_FILETYPES_PERL, 2);
	set_sci_style(sci, SCE_PL_NUMBER, GEANY_FILETYPES_PERL, 3);
	set_sci_style(sci, SCE_PL_WORD, GEANY_FILETYPES_PERL, 4);
	set_sci_style(sci, SCE_PL_STRING, GEANY_FILETYPES_PERL, 5);
	set_sci_style(sci, SCE_PL_CHARACTER, GEANY_FILETYPES_PERL, 6);
	set_sci_style(sci, SCE_PL_PREPROCESSOR, GEANY_FILETYPES_PERL, 7);
	set_sci_style(sci, SCE_PL_OPERATOR, GEANY_FILETYPES_PERL, 8);
	set_sci_style(sci, SCE_PL_IDENTIFIER, GEANY_FILETYPES_PERL, 9);
	set_sci_style(sci, SCE_PL_SCALAR, GEANY_FILETYPES_PERL, 10);
	set_sci_style(sci, SCE_PL_POD, GEANY_FILETYPES_PERL, 11);
	set_sci_style(sci, SCE_PL_REGEX, GEANY_FILETYPES_PERL, 12);
	set_sci_style(sci, SCE_PL_ARRAY, GEANY_FILETYPES_PERL, 13);
	set_sci_style(sci, SCE_PL_HASH, GEANY_FILETYPES_PERL, 14);
	set_sci_style(sci, SCE_PL_SYMBOLTABLE, GEANY_FILETYPES_PERL, 15);
	set_sci_style(sci, SCE_PL_BACKTICKS, GEANY_FILETYPES_PERL, 16);
	set_sci_style(sci, SCE_PL_POD_VERB, GEANY_FILETYPES_PERL, 17);
	set_sci_style(sci, SCE_PL_REGSUBST, GEANY_FILETYPES_PERL, 18);
	set_sci_style(sci, SCE_PL_DATASECTION, GEANY_FILETYPES_PERL, 19);
	set_sci_style(sci, SCE_PL_HERE_DELIM, GEANY_FILETYPES_PERL, 20);
	set_sci_style(sci, SCE_PL_HERE_Q, GEANY_FILETYPES_PERL, 21);
	set_sci_style(sci, SCE_PL_HERE_QQ, GEANY_FILETYPES_PERL, 22);
	set_sci_style(sci, SCE_PL_HERE_QX, GEANY_FILETYPES_PERL, 23);
	set_sci_style(sci, SCE_PL_STRING_Q, GEANY_FILETYPES_PERL, 24);
	set_sci_style(sci, SCE_PL_STRING_QQ, GEANY_FILETYPES_PERL, 25);
	set_sci_style(sci, SCE_PL_STRING_QX, GEANY_FILETYPES_PERL, 26);
	set_sci_style(sci, SCE_PL_STRING_QR, GEANY_FILETYPES_PERL, 27);
	set_sci_style(sci, SCE_PL_STRING_QW, GEANY_FILETYPES_PERL, 28);
	set_sci_style(sci, SCE_PL_VARIABLE_INDEXER, GEANY_FILETYPES_PERL, 29);
	set_sci_style(sci, SCE_PL_PUNCTUATION, GEANY_FILETYPES_PERL, 30);
	set_sci_style(sci, SCE_PL_LONGQUOTE, GEANY_FILETYPES_PERL, 31);
	set_sci_style(sci, SCE_PL_SUB_PROTOTYPE, GEANY_FILETYPES_PERL, 32);
	set_sci_style(sci, SCE_PL_FORMAT_IDENT, GEANY_FILETYPES_PERL, 33);
	set_sci_style(sci, SCE_PL_FORMAT, GEANY_FILETYPES_PERL, 34);
}


static void styleset_python_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_PYTHON, 16);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_PYTHON].styling[0]);
	get_keyfile_style(config, config_home, "commentline", &style_sets[GEANY_FILETYPES_PYTHON].styling[1]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_PYTHON].styling[2]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_PYTHON].styling[3]);
	get_keyfile_style(config, config_home, "character", &style_sets[GEANY_FILETYPES_PYTHON].styling[4]);
	get_keyfile_style(config, config_home, "word", &style_sets[GEANY_FILETYPES_PYTHON].styling[5]);
	get_keyfile_style(config, config_home, "triple", &style_sets[GEANY_FILETYPES_PYTHON].styling[6]);
	get_keyfile_style(config, config_home, "tripledouble", &style_sets[GEANY_FILETYPES_PYTHON].styling[7]);
	get_keyfile_style(config, config_home, "classname", &style_sets[GEANY_FILETYPES_PYTHON].styling[8]);
	get_keyfile_style(config, config_home, "defname", &style_sets[GEANY_FILETYPES_PYTHON].styling[9]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_PYTHON].styling[10]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_PYTHON].styling[11]);
	get_keyfile_style(config, config_home, "commentblock", &style_sets[GEANY_FILETYPES_PYTHON].styling[12]);
	get_keyfile_style(config, config_home, "stringeol", &style_sets[GEANY_FILETYPES_PYTHON].styling[13]);
	get_keyfile_style(config, config_home, "word2", &style_sets[GEANY_FILETYPES_PYTHON].styling[14]);
	get_keyfile_style(config, config_home, "decorator", &style_sets[GEANY_FILETYPES_PYTHON].styling[15]);

	style_sets[GEANY_FILETYPES_PYTHON].keywords = g_new(gchar*, 3);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_PYTHON, 0);
	get_keyfile_keywords(config, config_home, "identifiers", GEANY_FILETYPES_PYTHON, 1);
	style_sets[GEANY_FILETYPES_PYTHON].keywords[2] = NULL;
}


static void styleset_python(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_PYTHON;

	apply_filetype_properties(sci, SCLEX_PYTHON, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_PYTHON].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_PYTHON].keywords[1]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_PYTHON, 0);
	set_sci_style(sci, SCE_P_DEFAULT, GEANY_FILETYPES_PYTHON, 0);
	set_sci_style(sci, SCE_P_COMMENTLINE, GEANY_FILETYPES_PYTHON, 1);
	set_sci_style(sci, SCE_P_NUMBER, GEANY_FILETYPES_PYTHON, 2);
	set_sci_style(sci, SCE_P_STRING, GEANY_FILETYPES_PYTHON, 3);
	set_sci_style(sci, SCE_P_CHARACTER, GEANY_FILETYPES_PYTHON, 4);
	set_sci_style(sci, SCE_P_WORD, GEANY_FILETYPES_PYTHON, 5);
	set_sci_style(sci, SCE_P_TRIPLE, GEANY_FILETYPES_PYTHON, 6);
	set_sci_style(sci, SCE_P_TRIPLEDOUBLE, GEANY_FILETYPES_PYTHON, 7);
	set_sci_style(sci, SCE_P_CLASSNAME, GEANY_FILETYPES_PYTHON, 8);
	set_sci_style(sci, SCE_P_DEFNAME, GEANY_FILETYPES_PYTHON, 9);
	set_sci_style(sci, SCE_P_OPERATOR, GEANY_FILETYPES_PYTHON, 10);
	set_sci_style(sci, SCE_P_IDENTIFIER, GEANY_FILETYPES_PYTHON, 11);
	set_sci_style(sci, SCE_P_COMMENTBLOCK, GEANY_FILETYPES_PYTHON, 12);
	set_sci_style(sci, SCE_P_STRINGEOL, GEANY_FILETYPES_PYTHON, 13);
	set_sci_style(sci, SCE_P_WORD2, GEANY_FILETYPES_PYTHON, 14);
	set_sci_style(sci, SCE_P_DECORATOR, GEANY_FILETYPES_PYTHON, 15);

	SSM(sci, SCI_SETPROPERTY, (uptr_t) "fold.comment.python", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "fold.quotes.python", (sptr_t) "1");
}


static void styleset_cmake_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_CMAKE, 15);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_CMAKE].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_CMAKE].styling[1]);
	get_keyfile_style(config, config_home, "stringdq", &style_sets[GEANY_FILETYPES_CMAKE].styling[2]);
	get_keyfile_style(config, config_home, "stringlq", &style_sets[GEANY_FILETYPES_CMAKE].styling[3]);
	get_keyfile_style(config, config_home, "stringrq", &style_sets[GEANY_FILETYPES_CMAKE].styling[4]);
	get_keyfile_style(config, config_home, "command", &style_sets[GEANY_FILETYPES_CMAKE].styling[5]);
	get_keyfile_style(config, config_home, "parameters", &style_sets[GEANY_FILETYPES_CMAKE].styling[6]);
	get_keyfile_style(config, config_home, "variable", &style_sets[GEANY_FILETYPES_CMAKE].styling[7]);
	get_keyfile_style(config, config_home, "userdefined", &style_sets[GEANY_FILETYPES_CMAKE].styling[8]);
	get_keyfile_style(config, config_home, "whiledef", &style_sets[GEANY_FILETYPES_CMAKE].styling[9]);
	get_keyfile_style(config, config_home, "foreachdef", &style_sets[GEANY_FILETYPES_CMAKE].styling[10]);
	get_keyfile_style(config, config_home, "ifdefinedef", &style_sets[GEANY_FILETYPES_CMAKE].styling[11]);
	get_keyfile_style(config, config_home, "macrodef", &style_sets[GEANY_FILETYPES_CMAKE].styling[12]);
	get_keyfile_style(config, config_home, "stringvar", &style_sets[GEANY_FILETYPES_CMAKE].styling[13]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_CMAKE].styling[14]);

	style_sets[GEANY_FILETYPES_CMAKE].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "commands", GEANY_FILETYPES_CMAKE, 0);
	get_keyfile_keywords(config, config_home, "parameters", GEANY_FILETYPES_CMAKE, 1);
	get_keyfile_keywords(config, config_home, "userdefined", GEANY_FILETYPES_CMAKE, 2);
	style_sets[GEANY_FILETYPES_CMAKE].keywords[3] = NULL;
}


static void styleset_cmake(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_CMAKE;

	apply_filetype_properties(sci, SCLEX_CMAKE, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_CMAKE].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_CMAKE].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_CMAKE].keywords[2]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_CMAKE, 0);
	set_sci_style(sci, SCE_CMAKE_DEFAULT, GEANY_FILETYPES_CMAKE, 0);
	set_sci_style(sci, SCE_CMAKE_COMMENT, GEANY_FILETYPES_CMAKE, 1);
	set_sci_style(sci, SCE_CMAKE_STRINGDQ, GEANY_FILETYPES_CMAKE, 2);
	set_sci_style(sci, SCE_CMAKE_STRINGLQ, GEANY_FILETYPES_CMAKE, 3);
	set_sci_style(sci, SCE_CMAKE_STRINGRQ, GEANY_FILETYPES_CMAKE, 4);
	set_sci_style(sci, SCE_CMAKE_COMMANDS, GEANY_FILETYPES_CMAKE, 5);
	set_sci_style(sci, SCE_CMAKE_PARAMETERS, GEANY_FILETYPES_CMAKE, 6);
	set_sci_style(sci, SCE_CMAKE_VARIABLE, GEANY_FILETYPES_CMAKE, 7);
	set_sci_style(sci, SCE_CMAKE_USERDEFINED, GEANY_FILETYPES_CMAKE, 8);
	set_sci_style(sci, SCE_CMAKE_WHILEDEF, GEANY_FILETYPES_CMAKE, 9);
	set_sci_style(sci, SCE_CMAKE_FOREACHDEF, GEANY_FILETYPES_CMAKE, 10);
	set_sci_style(sci, SCE_CMAKE_IFDEFINEDEF, GEANY_FILETYPES_CMAKE, 11);
	set_sci_style(sci, SCE_CMAKE_MACRODEF, GEANY_FILETYPES_CMAKE, 12);
	set_sci_style(sci, SCE_CMAKE_STRINGVAR, GEANY_FILETYPES_CMAKE, 13);
	set_sci_style(sci, SCE_CMAKE_NUMBER, GEANY_FILETYPES_CMAKE, 14);
}


static void styleset_r_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_R, 12);

	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_R].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_R].styling[1]);
	get_keyfile_style(config, config_home, "kword", &style_sets[GEANY_FILETYPES_R].styling[2]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_R].styling[3]);
	get_keyfile_style(config, config_home, "basekword", &style_sets[GEANY_FILETYPES_R].styling[4]);
	get_keyfile_style(config, config_home, "otherkword", &style_sets[GEANY_FILETYPES_R].styling[5]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_R].styling[6]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_R].styling[7]);
	get_keyfile_style(config, config_home, "string2", &style_sets[GEANY_FILETYPES_R].styling[8]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_R].styling[9]);
	get_keyfile_style(config, config_home, "infix", &style_sets[GEANY_FILETYPES_R].styling[10]);
	get_keyfile_style(config, config_home, "infixeol", &style_sets[GEANY_FILETYPES_R].styling[11]);

	style_sets[GEANY_FILETYPES_R].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_R, 0);
	get_keyfile_keywords(config, config_home, "package", GEANY_FILETYPES_R, 1);
	get_keyfile_keywords(config, config_home, "package_other", GEANY_FILETYPES_R, 2);
	style_sets[GEANY_FILETYPES_R].keywords[3] = NULL;
}


static void styleset_r(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_R;

	apply_filetype_properties(sci, SCLEX_R, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_R].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_R].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_R].keywords[2]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_R, 0);
	set_sci_style(sci, SCE_R_DEFAULT, GEANY_FILETYPES_R, 0);
	set_sci_style(sci, SCE_R_COMMENT, GEANY_FILETYPES_R, 1);
	set_sci_style(sci, SCE_R_KWORD, GEANY_FILETYPES_R, 2);
	set_sci_style(sci, SCE_R_OPERATOR, GEANY_FILETYPES_R, 3);
	set_sci_style(sci, SCE_R_BASEKWORD, GEANY_FILETYPES_R, 4);
	set_sci_style(sci, SCE_R_OTHERKWORD, GEANY_FILETYPES_R, 5);
	set_sci_style(sci, SCE_R_NUMBER, GEANY_FILETYPES_R, 6);
	set_sci_style(sci, SCE_R_STRING, GEANY_FILETYPES_R, 7);
	set_sci_style(sci, SCE_R_STRING2, GEANY_FILETYPES_R, 8);
	set_sci_style(sci, SCE_R_IDENTIFIER, GEANY_FILETYPES_R, 9);
	set_sci_style(sci, SCE_R_INFIX, GEANY_FILETYPES_R, 10);
	set_sci_style(sci, SCE_R_INFIXEOL, GEANY_FILETYPES_R, 11);
}


static void styleset_ruby_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_RUBY, 35);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_RUBY].styling[0]);
	get_keyfile_style(config, config_home, "commentline", &style_sets[GEANY_FILETYPES_RUBY].styling[1]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_RUBY].styling[2]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_RUBY].styling[3]);
	get_keyfile_style(config, config_home, "character", &style_sets[GEANY_FILETYPES_RUBY].styling[4]);
	get_keyfile_style(config, config_home, "word", &style_sets[GEANY_FILETYPES_RUBY].styling[5]);
	get_keyfile_style(config, config_home, "global", &style_sets[GEANY_FILETYPES_RUBY].styling[6]);
	get_keyfile_style(config, config_home, "symbol", &style_sets[GEANY_FILETYPES_RUBY].styling[7]);
	get_keyfile_style(config, config_home, "classname", &style_sets[GEANY_FILETYPES_RUBY].styling[8]);
	get_keyfile_style(config, config_home, "defname", &style_sets[GEANY_FILETYPES_RUBY].styling[9]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_RUBY].styling[10]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_RUBY].styling[11]);
	get_keyfile_style(config, config_home, "modulename", &style_sets[GEANY_FILETYPES_RUBY].styling[12]);
	get_keyfile_style(config, config_home, "backticks", &style_sets[GEANY_FILETYPES_RUBY].styling[13]);
	get_keyfile_style(config, config_home, "instancevar", &style_sets[GEANY_FILETYPES_RUBY].styling[14]);
	get_keyfile_style(config, config_home, "classvar", &style_sets[GEANY_FILETYPES_RUBY].styling[15]);
	get_keyfile_style(config, config_home, "datasection", &style_sets[GEANY_FILETYPES_RUBY].styling[16]);
	get_keyfile_style(config, config_home, "heredelim", &style_sets[GEANY_FILETYPES_RUBY].styling[17]);
	get_keyfile_style(config, config_home, "worddemoted", &style_sets[GEANY_FILETYPES_RUBY].styling[18]);
	get_keyfile_style(config, config_home, "stdin", &style_sets[GEANY_FILETYPES_RUBY].styling[19]);
	get_keyfile_style(config, config_home, "stdout", &style_sets[GEANY_FILETYPES_RUBY].styling[20]);
	get_keyfile_style(config, config_home, "stderr", &style_sets[GEANY_FILETYPES_RUBY].styling[21]);
	get_keyfile_style(config, config_home, "datasection", &style_sets[GEANY_FILETYPES_RUBY].styling[22]);
	get_keyfile_style(config, config_home, "regex", &style_sets[GEANY_FILETYPES_RUBY].styling[23]);
	get_keyfile_style(config, config_home, "here_q", &style_sets[GEANY_FILETYPES_RUBY].styling[24]);
	get_keyfile_style(config, config_home, "here_qq", &style_sets[GEANY_FILETYPES_RUBY].styling[25]);
	get_keyfile_style(config, config_home, "here_qx", &style_sets[GEANY_FILETYPES_RUBY].styling[26]);
	get_keyfile_style(config, config_home, "string_q", &style_sets[GEANY_FILETYPES_RUBY].styling[27]);
	get_keyfile_style(config, config_home, "string_qq", &style_sets[GEANY_FILETYPES_RUBY].styling[28]);
	get_keyfile_style(config, config_home, "string_qx", &style_sets[GEANY_FILETYPES_RUBY].styling[29]);
	get_keyfile_style(config, config_home, "string_qr", &style_sets[GEANY_FILETYPES_RUBY].styling[30]);
	get_keyfile_style(config, config_home, "string_qw", &style_sets[GEANY_FILETYPES_RUBY].styling[31]);
	get_keyfile_style(config, config_home, "upper_bound", &style_sets[GEANY_FILETYPES_RUBY].styling[32]);
	get_keyfile_style(config, config_home, "error", &style_sets[GEANY_FILETYPES_RUBY].styling[33]);
	get_keyfile_style(config, config_home, "pod", &style_sets[GEANY_FILETYPES_RUBY].styling[34]);

	style_sets[GEANY_FILETYPES_RUBY].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_RUBY, 0);
	style_sets[GEANY_FILETYPES_RUBY].keywords[1] = NULL;
}


static void styleset_ruby(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_RUBY;

	apply_filetype_properties(sci, SCLEX_RUBY, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_RUBY].keywords[0]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_RUBY, 0);
	set_sci_style(sci, SCE_RB_DEFAULT, GEANY_FILETYPES_RUBY, 0);
	set_sci_style(sci, SCE_RB_COMMENTLINE, GEANY_FILETYPES_RUBY, 1);
	set_sci_style(sci, SCE_RB_NUMBER, GEANY_FILETYPES_RUBY, 2);
	set_sci_style(sci, SCE_RB_STRING, GEANY_FILETYPES_RUBY, 3);
	set_sci_style(sci, SCE_RB_CHARACTER, GEANY_FILETYPES_RUBY, 4);
	set_sci_style(sci, SCE_RB_WORD, GEANY_FILETYPES_RUBY, 5);
	set_sci_style(sci, SCE_RB_GLOBAL, GEANY_FILETYPES_RUBY, 6);
	set_sci_style(sci, SCE_RB_SYMBOL, GEANY_FILETYPES_RUBY, 7);
	set_sci_style(sci, SCE_RB_CLASSNAME, GEANY_FILETYPES_RUBY, 8);
	set_sci_style(sci, SCE_RB_DEFNAME, GEANY_FILETYPES_RUBY, 9);
	set_sci_style(sci, SCE_RB_OPERATOR, GEANY_FILETYPES_RUBY, 10);
	set_sci_style(sci, SCE_RB_IDENTIFIER, GEANY_FILETYPES_RUBY, 11);
	set_sci_style(sci, SCE_RB_MODULE_NAME, GEANY_FILETYPES_RUBY, 12);
	set_sci_style(sci, SCE_RB_BACKTICKS, GEANY_FILETYPES_RUBY, 13);
	set_sci_style(sci, SCE_RB_INSTANCE_VAR, GEANY_FILETYPES_RUBY, 14);
	set_sci_style(sci, SCE_RB_CLASS_VAR, GEANY_FILETYPES_RUBY, 15);
	set_sci_style(sci, SCE_RB_DATASECTION, GEANY_FILETYPES_RUBY, 16);
	set_sci_style(sci, SCE_RB_HERE_DELIM, GEANY_FILETYPES_RUBY, 17);
	set_sci_style(sci, SCE_RB_WORD_DEMOTED, GEANY_FILETYPES_RUBY, 18);
	set_sci_style(sci, SCE_RB_STDIN, GEANY_FILETYPES_RUBY, 19);
	set_sci_style(sci, SCE_RB_STDOUT, GEANY_FILETYPES_RUBY, 20);
	set_sci_style(sci, SCE_RB_STDERR, GEANY_FILETYPES_RUBY, 21);
	set_sci_style(sci, SCE_RB_DATASECTION, GEANY_FILETYPES_RUBY, 22);
	set_sci_style(sci, SCE_RB_REGEX, GEANY_FILETYPES_RUBY, 23);
	set_sci_style(sci, SCE_RB_HERE_Q, GEANY_FILETYPES_RUBY, 24);
	set_sci_style(sci, SCE_RB_HERE_QQ, GEANY_FILETYPES_RUBY, 25);
	set_sci_style(sci, SCE_RB_HERE_QX, GEANY_FILETYPES_RUBY, 26);
	set_sci_style(sci, SCE_RB_STRING_Q, GEANY_FILETYPES_RUBY, 27);
	set_sci_style(sci, SCE_RB_STRING_QQ, GEANY_FILETYPES_RUBY, 28);
	set_sci_style(sci, SCE_RB_STRING_QX, GEANY_FILETYPES_RUBY, 29);
	set_sci_style(sci, SCE_RB_STRING_QR, GEANY_FILETYPES_RUBY, 30);
	set_sci_style(sci, SCE_RB_STRING_QW, GEANY_FILETYPES_RUBY, 31);
	set_sci_style(sci, SCE_RB_UPPER_BOUND, GEANY_FILETYPES_RUBY, 32);
	set_sci_style(sci, SCE_RB_ERROR, GEANY_FILETYPES_RUBY, 33);
	set_sci_style(sci, SCE_RB_POD, GEANY_FILETYPES_RUBY, 34);
}


static void styleset_sh_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_SH, 14);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_SH].styling[0]);
	get_keyfile_style(config, config_home, "commentline", &style_sets[GEANY_FILETYPES_SH].styling[1]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_SH].styling[2]);
	get_keyfile_style(config, config_home, "word", &style_sets[GEANY_FILETYPES_SH].styling[3]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_SH].styling[4]);
	get_keyfile_style(config, config_home, "character", &style_sets[GEANY_FILETYPES_SH].styling[5]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_SH].styling[6]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_SH].styling[7]);
	get_keyfile_style(config, config_home, "backticks", &style_sets[GEANY_FILETYPES_SH].styling[8]);
	get_keyfile_style(config, config_home, "param", &style_sets[GEANY_FILETYPES_SH].styling[9]);
	get_keyfile_style(config, config_home, "scalar", &style_sets[GEANY_FILETYPES_SH].styling[10]);
	get_keyfile_style(config, config_home, "error", &style_sets[GEANY_FILETYPES_SH].styling[11]);
	get_keyfile_style(config, config_home, "here_delim", &style_sets[GEANY_FILETYPES_SH].styling[12]);
	get_keyfile_style(config, config_home, "here_q", &style_sets[GEANY_FILETYPES_SH].styling[13]);

	style_sets[GEANY_FILETYPES_SH].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_SH, 0);
	style_sets[GEANY_FILETYPES_SH].keywords[1] = NULL;
}


static void styleset_sh(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_SH;

	apply_filetype_properties(sci, SCLEX_BASH, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_SH].keywords[0]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_SH, 0);
	set_sci_style(sci, SCE_SH_DEFAULT, GEANY_FILETYPES_SH, 0);
	set_sci_style(sci, SCE_SH_COMMENTLINE, GEANY_FILETYPES_SH, 1);
	set_sci_style(sci, SCE_SH_NUMBER, GEANY_FILETYPES_SH, 2);
	set_sci_style(sci, SCE_SH_WORD, GEANY_FILETYPES_SH, 3);
	set_sci_style(sci, SCE_SH_STRING, GEANY_FILETYPES_SH, 4);
	set_sci_style(sci, SCE_SH_CHARACTER, GEANY_FILETYPES_SH, 5);
	set_sci_style(sci, SCE_SH_OPERATOR, GEANY_FILETYPES_SH, 6);
	set_sci_style(sci, SCE_SH_IDENTIFIER, GEANY_FILETYPES_SH, 7);
	set_sci_style(sci, SCE_SH_BACKTICKS, GEANY_FILETYPES_SH, 8);
	set_sci_style(sci, SCE_SH_PARAM, GEANY_FILETYPES_SH, 9);
	set_sci_style(sci, SCE_SH_SCALAR, GEANY_FILETYPES_SH, 10);
	set_sci_style(sci, SCE_SH_ERROR, GEANY_FILETYPES_SH, 11);
	set_sci_style(sci, SCE_SH_HERE_DELIM, GEANY_FILETYPES_SH, 12);
	set_sci_style(sci, SCE_SH_HERE_Q, GEANY_FILETYPES_SH, 13);
}


static void styleset_xml(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_XML;

	apply_filetype_properties(sci, SCLEX_XML, ft_id);

	/* use the same colouring for HTML; XML and so on */
	styleset_markup(sci, FALSE);
}


static void styleset_docbook_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_DOCBOOK, 29);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[0]);
	get_keyfile_style(config, config_home, "tag", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[1]);
	get_keyfile_style(config, config_home, "tagunknown", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[2]);
	get_keyfile_style(config, config_home, "attribute", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[3]);
	get_keyfile_style(config, config_home, "attributeunknown", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[4]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[5]);
	get_keyfile_style(config, config_home, "doublestring", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[6]);
	get_keyfile_style(config, config_home, "singlestring", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[7]);
	get_keyfile_style(config, config_home, "other", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[8]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[9]);
	get_keyfile_style(config, config_home, "entity", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[10]);
	get_keyfile_style(config, config_home, "tagend", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[11]);
	get_keyfile_style(config, config_home, "xmlstart", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[12]);
	get_keyfile_style(config, config_home, "xmlend", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[13]);
	get_keyfile_style(config, config_home, "cdata", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[14]);
	get_keyfile_style(config, config_home, "question", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[15]);
	get_keyfile_style(config, config_home, "value", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[16]);
	get_keyfile_style(config, config_home, "xccomment", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[17]);
	get_keyfile_style(config, config_home, "sgml_default", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[18]);
	get_keyfile_style(config, config_home, "sgml_comment", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[19]);
	get_keyfile_style(config, config_home, "sgml_special", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[20]);
	get_keyfile_style(config, config_home, "sgml_command", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[21]);
	get_keyfile_style(config, config_home, "sgml_doublestring", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[22]);
	get_keyfile_style(config, config_home, "sgml_simplestring", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[23]);
	get_keyfile_style(config, config_home, "sgml_1st_param", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[24]);
	get_keyfile_style(config, config_home, "sgml_entity", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[25]);
	get_keyfile_style(config, config_home, "sgml_block_default", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[26]);
	get_keyfile_style(config, config_home, "sgml_1st_param_comment", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[27]);
	get_keyfile_style(config, config_home, "sgml_error", &style_sets[GEANY_FILETYPES_DOCBOOK].styling[28]);

	style_sets[GEANY_FILETYPES_DOCBOOK].keywords = g_new(gchar*, 3);
	get_keyfile_keywords(config, config_home, "elements", GEANY_FILETYPES_DOCBOOK, 0);
	get_keyfile_keywords(config, config_home, "dtd", GEANY_FILETYPES_DOCBOOK, 1);
	style_sets[GEANY_FILETYPES_DOCBOOK].keywords[2] = NULL;
}


static void styleset_docbook(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_DOCBOOK;

	apply_filetype_properties(sci, SCLEX_XML, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_DOCBOOK].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 5, (sptr_t) style_sets[GEANY_FILETYPES_DOCBOOK].keywords[1]);

	/* Unknown tags and attributes are highlighed in red.
	 * If a tag is actually OK, it should be added in lower case to the htmlKeyWords string. */

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_DOCBOOK, 0);
	set_sci_style(sci, SCE_H_DEFAULT, GEANY_FILETYPES_DOCBOOK, 0);
	set_sci_style(sci, SCE_H_TAG, GEANY_FILETYPES_DOCBOOK, 1);
	set_sci_style(sci, SCE_H_TAGUNKNOWN, GEANY_FILETYPES_DOCBOOK, 2);
	set_sci_style(sci, SCE_H_ATTRIBUTE, GEANY_FILETYPES_DOCBOOK, 3);
	set_sci_style(sci, SCE_H_ATTRIBUTEUNKNOWN, GEANY_FILETYPES_DOCBOOK, 4);
	set_sci_style(sci, SCE_H_NUMBER, GEANY_FILETYPES_DOCBOOK, 5);
	set_sci_style(sci, SCE_H_DOUBLESTRING, GEANY_FILETYPES_DOCBOOK, 6);
	set_sci_style(sci, SCE_H_SINGLESTRING, GEANY_FILETYPES_DOCBOOK, 7);
	set_sci_style(sci, SCE_H_OTHER, GEANY_FILETYPES_DOCBOOK, 8);
	set_sci_style(sci, SCE_H_COMMENT, GEANY_FILETYPES_DOCBOOK, 9);
	set_sci_style(sci, SCE_H_ENTITY, GEANY_FILETYPES_DOCBOOK, 10);
	set_sci_style(sci, SCE_H_TAGEND, GEANY_FILETYPES_DOCBOOK, 11);
	SSM(sci, SCI_STYLESETEOLFILLED, SCE_H_XMLSTART, 1);
	set_sci_style(sci, SCE_H_XMLSTART, GEANY_FILETYPES_DOCBOOK, 12);
	set_sci_style(sci, SCE_H_XMLEND, GEANY_FILETYPES_DOCBOOK, 13);
	set_sci_style(sci, SCE_H_CDATA, GEANY_FILETYPES_DOCBOOK, 14);
	set_sci_style(sci, SCE_H_QUESTION, GEANY_FILETYPES_DOCBOOK, 15);
	set_sci_style(sci, SCE_H_VALUE, GEANY_FILETYPES_DOCBOOK, 16);
	set_sci_style(sci, SCE_H_XCCOMMENT, GEANY_FILETYPES_DOCBOOK, 17);
	set_sci_style(sci, SCE_H_SGML_DEFAULT, GEANY_FILETYPES_DOCBOOK, 18);
	set_sci_style(sci, SCE_H_DEFAULT, GEANY_FILETYPES_DOCBOOK, 19);
	set_sci_style(sci, SCE_H_SGML_SPECIAL, GEANY_FILETYPES_DOCBOOK, 20);
	set_sci_style(sci, SCE_H_SGML_COMMAND, GEANY_FILETYPES_DOCBOOK, 21);
	set_sci_style(sci, SCE_H_SGML_DOUBLESTRING, GEANY_FILETYPES_DOCBOOK, 22);
	set_sci_style(sci, SCE_H_SGML_SIMPLESTRING, GEANY_FILETYPES_DOCBOOK, 23);
	set_sci_style(sci, SCE_H_SGML_1ST_PARAM, GEANY_FILETYPES_DOCBOOK, 24);
	set_sci_style(sci, SCE_H_SGML_ENTITY, GEANY_FILETYPES_DOCBOOK, 25);
	set_sci_style(sci, SCE_H_SGML_BLOCK_DEFAULT, GEANY_FILETYPES_DOCBOOK, 26);
	set_sci_style(sci, SCE_H_SGML_1ST_PARAM_COMMENT, GEANY_FILETYPES_DOCBOOK, 27);
	set_sci_style(sci, SCE_H_SGML_ERROR, GEANY_FILETYPES_DOCBOOK, 28);

	SSM(sci, SCI_SETPROPERTY, (uptr_t) "fold.html", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "fold.html.preprocessor", (sptr_t) "1");
}


static void styleset_default(ScintillaObject *sci, gint ft_id)
{
	SSM(sci, SCI_SETLEXER, SCLEX_NULL, 0);

	/* we need to set STYLE_DEFAULT before we call SCI_STYLECLEARALL in styleset_common() */
	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_NONE, GCS_DEFAULT);

	styleset_common(sci, ft_id);
}


static void styleset_css_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_CSS, 22);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_CSS].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_CSS].styling[1]);
	get_keyfile_style(config, config_home, "tag", &style_sets[GEANY_FILETYPES_CSS].styling[2]);
	get_keyfile_style(config, config_home, "class", &style_sets[GEANY_FILETYPES_CSS].styling[3]);
	get_keyfile_style(config, config_home, "pseudoclass", &style_sets[GEANY_FILETYPES_CSS].styling[4]);
	get_keyfile_style(config, config_home, "unknown_pseudoclass", &style_sets[GEANY_FILETYPES_CSS].styling[5]);
	get_keyfile_style(config, config_home, "unknown_identifier", &style_sets[GEANY_FILETYPES_CSS].styling[6]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_CSS].styling[7]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_CSS].styling[8]);
	get_keyfile_style(config, config_home, "doublestring", &style_sets[GEANY_FILETYPES_CSS].styling[9]);
	get_keyfile_style(config, config_home, "singlestring", &style_sets[GEANY_FILETYPES_CSS].styling[10]);
	get_keyfile_style(config, config_home, "attribute", &style_sets[GEANY_FILETYPES_CSS].styling[11]);
	get_keyfile_style(config, config_home, "value", &style_sets[GEANY_FILETYPES_CSS].styling[12]);
	get_keyfile_style(config, config_home, "id", &style_sets[GEANY_FILETYPES_CSS].styling[13]);
	get_keyfile_style(config, config_home, "identifier2", &style_sets[GEANY_FILETYPES_CSS].styling[14]);
	get_keyfile_style(config, config_home, "important", &style_sets[GEANY_FILETYPES_CSS].styling[15]);
	get_keyfile_style(config, config_home, "directive", &style_sets[GEANY_FILETYPES_CSS].styling[16]);
	get_keyfile_style(config, config_home, "identifier3", &style_sets[GEANY_FILETYPES_CSS].styling[17]);
	get_keyfile_style(config, config_home, "pseudoelement", &style_sets[GEANY_FILETYPES_CSS].styling[18]);
	get_keyfile_style(config, config_home, "extended_identifier", &style_sets[GEANY_FILETYPES_CSS].styling[19]);
	get_keyfile_style(config, config_home, "extended_pseudoclass", &style_sets[GEANY_FILETYPES_CSS].styling[20]);
	get_keyfile_style(config, config_home, "extended_pseudoelement", &style_sets[GEANY_FILETYPES_CSS].styling[21]);

	style_sets[GEANY_FILETYPES_CSS].keywords = g_new(gchar*, 9);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_CSS, 0);
	get_keyfile_keywords(config, config_home, "pseudoclasses", GEANY_FILETYPES_CSS, 1);
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_CSS, 2);
	get_keyfile_keywords(config, config_home, "css3_properties", GEANY_FILETYPES_CSS, 3);
	get_keyfile_keywords(config, config_home, "pseudo_elements", GEANY_FILETYPES_CSS, 4);
	get_keyfile_keywords(config, config_home, "browser_css_properties", GEANY_FILETYPES_CSS, 5);
	get_keyfile_keywords(config, config_home, "browser_pseudo_classes", GEANY_FILETYPES_CSS, 6);
	get_keyfile_keywords(config, config_home, "browser_pseudo_elements", GEANY_FILETYPES_CSS, 7);
	style_sets[GEANY_FILETYPES_CSS].keywords[8] = NULL;
}


static void styleset_css(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_CSS;
	guint i;

	apply_filetype_properties(sci, SCLEX_CSS, ft_id);

	for (i = 0; i < 8; i++)
	{
		SSM(sci, SCI_SETKEYWORDS, i, (sptr_t) style_sets[GEANY_FILETYPES_CSS].keywords[i]);
	}

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_CSS, 0);
	set_sci_style(sci, SCE_CSS_DEFAULT, GEANY_FILETYPES_CSS, 0);
	set_sci_style(sci, SCE_CSS_COMMENT, GEANY_FILETYPES_CSS, 1);
	set_sci_style(sci, SCE_CSS_TAG, GEANY_FILETYPES_CSS, 2);
	set_sci_style(sci, SCE_CSS_CLASS, GEANY_FILETYPES_CSS, 3);
	set_sci_style(sci, SCE_CSS_PSEUDOCLASS, GEANY_FILETYPES_CSS, 4);
	set_sci_style(sci, SCE_CSS_UNKNOWN_PSEUDOCLASS, GEANY_FILETYPES_CSS, 5);
	set_sci_style(sci, SCE_CSS_UNKNOWN_IDENTIFIER, GEANY_FILETYPES_CSS, 6);
	set_sci_style(sci, SCE_CSS_OPERATOR, GEANY_FILETYPES_CSS, 7);
	set_sci_style(sci, SCE_CSS_IDENTIFIER, GEANY_FILETYPES_CSS, 8);
	set_sci_style(sci, SCE_CSS_DOUBLESTRING, GEANY_FILETYPES_CSS, 9);
	set_sci_style(sci, SCE_CSS_SINGLESTRING, GEANY_FILETYPES_CSS, 10);
	set_sci_style(sci, SCE_CSS_ATTRIBUTE, GEANY_FILETYPES_CSS, 11);
	set_sci_style(sci, SCE_CSS_VALUE, GEANY_FILETYPES_CSS, 12);
	set_sci_style(sci, SCE_CSS_ID, GEANY_FILETYPES_CSS, 13);
	set_sci_style(sci, SCE_CSS_IDENTIFIER2, GEANY_FILETYPES_CSS, 14);
	set_sci_style(sci, SCE_CSS_IMPORTANT, GEANY_FILETYPES_CSS, 15);
	set_sci_style(sci, SCE_CSS_DIRECTIVE, GEANY_FILETYPES_CSS, 16);
	set_sci_style(sci, SCE_CSS_IDENTIFIER3, GEANY_FILETYPES_CSS, 17);
	set_sci_style(sci, SCE_CSS_PSEUDOELEMENT, GEANY_FILETYPES_CSS, 18);
	set_sci_style(sci, SCE_CSS_EXTENDED_IDENTIFIER, GEANY_FILETYPES_CSS, 19);
	set_sci_style(sci, SCE_CSS_EXTENDED_PSEUDOCLASS, GEANY_FILETYPES_CSS, 20);
	set_sci_style(sci, SCE_CSS_EXTENDED_PSEUDOELEMENT, GEANY_FILETYPES_CSS, 21);
}


static void styleset_nsis_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_NSIS, 19);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_NSIS].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_NSIS].styling[1]);
	get_keyfile_style(config, config_home, "stringdq", &style_sets[GEANY_FILETYPES_NSIS].styling[2]);
	get_keyfile_style(config, config_home, "stringlq", &style_sets[GEANY_FILETYPES_NSIS].styling[3]);
	get_keyfile_style(config, config_home, "stringrq", &style_sets[GEANY_FILETYPES_NSIS].styling[4]);
	get_keyfile_style(config, config_home, "function", &style_sets[GEANY_FILETYPES_NSIS].styling[5]);
	get_keyfile_style(config, config_home, "variable", &style_sets[GEANY_FILETYPES_NSIS].styling[6]);
	get_keyfile_style(config, config_home, "label", &style_sets[GEANY_FILETYPES_NSIS].styling[7]);
	get_keyfile_style(config, config_home, "userdefined", &style_sets[GEANY_FILETYPES_NSIS].styling[8]);
	get_keyfile_style(config, config_home, "sectiondef", &style_sets[GEANY_FILETYPES_NSIS].styling[9]);
	get_keyfile_style(config, config_home, "subsectiondef", &style_sets[GEANY_FILETYPES_NSIS].styling[10]);
	get_keyfile_style(config, config_home, "ifdefinedef", &style_sets[GEANY_FILETYPES_NSIS].styling[11]);
	get_keyfile_style(config, config_home, "macrodef", &style_sets[GEANY_FILETYPES_NSIS].styling[12]);
	get_keyfile_style(config, config_home, "stringvar", &style_sets[GEANY_FILETYPES_NSIS].styling[13]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_NSIS].styling[14]);
	get_keyfile_style(config, config_home, "sectiongroup", &style_sets[GEANY_FILETYPES_NSIS].styling[15]);
	get_keyfile_style(config, config_home, "pageex", &style_sets[GEANY_FILETYPES_NSIS].styling[16]);
	get_keyfile_style(config, config_home, "functiondef", &style_sets[GEANY_FILETYPES_NSIS].styling[17]);
	get_keyfile_style(config, config_home, "commentbox", &style_sets[GEANY_FILETYPES_NSIS].styling[18]);

	style_sets[GEANY_FILETYPES_NSIS].keywords = g_new(gchar*, 5);
	get_keyfile_keywords(config, config_home, "functions", GEANY_FILETYPES_NSIS, 0);
	get_keyfile_keywords(config, config_home, "variables", GEANY_FILETYPES_NSIS, 1);
	get_keyfile_keywords(config, config_home, "lables", GEANY_FILETYPES_NSIS, 2);
	get_keyfile_keywords(config, config_home, "userdefined", GEANY_FILETYPES_NSIS, 3);
	style_sets[GEANY_FILETYPES_NSIS].keywords[4] = NULL;
}


static void styleset_nsis(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_NSIS;

	apply_filetype_properties(sci, SCLEX_NSIS, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_NSIS].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_NSIS].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_NSIS].keywords[2]);
	SSM(sci, SCI_SETKEYWORDS, 3, (sptr_t) style_sets[GEANY_FILETYPES_NSIS].keywords[3]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_NSIS, 0);
	set_sci_style(sci, SCE_NSIS_DEFAULT, GEANY_FILETYPES_NSIS, 0);
	set_sci_style(sci, SCE_NSIS_COMMENT, GEANY_FILETYPES_NSIS, 1);
	set_sci_style(sci, SCE_NSIS_STRINGDQ, GEANY_FILETYPES_NSIS, 2);
	set_sci_style(sci, SCE_NSIS_STRINGLQ, GEANY_FILETYPES_NSIS, 3);
	set_sci_style(sci, SCE_NSIS_STRINGRQ, GEANY_FILETYPES_NSIS, 4);
	set_sci_style(sci, SCE_NSIS_FUNCTION, GEANY_FILETYPES_NSIS, 5);
	set_sci_style(sci, SCE_NSIS_VARIABLE, GEANY_FILETYPES_NSIS, 6);
	set_sci_style(sci, SCE_NSIS_LABEL, GEANY_FILETYPES_NSIS, 7);
	set_sci_style(sci, SCE_NSIS_USERDEFINED, GEANY_FILETYPES_NSIS, 8);
	set_sci_style(sci, SCE_NSIS_SECTIONDEF, GEANY_FILETYPES_NSIS, 9);
	set_sci_style(sci, SCE_NSIS_SUBSECTIONDEF, GEANY_FILETYPES_NSIS, 10);
	set_sci_style(sci, SCE_NSIS_IFDEFINEDEF, GEANY_FILETYPES_NSIS, 11);
	set_sci_style(sci, SCE_NSIS_MACRODEF, GEANY_FILETYPES_NSIS, 12);
	set_sci_style(sci, SCE_NSIS_STRINGVAR, GEANY_FILETYPES_NSIS, 13);
	set_sci_style(sci, SCE_NSIS_NUMBER, GEANY_FILETYPES_NSIS, 14);
	set_sci_style(sci, SCE_NSIS_SECTIONGROUP, GEANY_FILETYPES_NSIS, 15);
	set_sci_style(sci, SCE_NSIS_PAGEEX, GEANY_FILETYPES_NSIS, 16);
	set_sci_style(sci, SCE_NSIS_FUNCTIONDEF, GEANY_FILETYPES_NSIS, 17);
	set_sci_style(sci, SCE_NSIS_COMMENTBOX, GEANY_FILETYPES_NSIS, 18);

	SSM(sci, SCI_SETPROPERTY, (uptr_t) "nsis.uservars", (sptr_t) "1");
	SSM(sci, SCI_SETPROPERTY, (uptr_t) "nsis.ignorecase", (sptr_t) "1");
}


static void styleset_po_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_PO, 9);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_PO].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_PO].styling[1]);
	get_keyfile_style(config, config_home, "msgid", &style_sets[GEANY_FILETYPES_PO].styling[2]);
	get_keyfile_style(config, config_home, "msgid_text", &style_sets[GEANY_FILETYPES_PO].styling[3]);
	get_keyfile_style(config, config_home, "msgstr", &style_sets[GEANY_FILETYPES_PO].styling[4]);
	get_keyfile_style(config, config_home, "msgstr_text", &style_sets[GEANY_FILETYPES_PO].styling[5]);
	get_keyfile_style(config, config_home, "msgctxt", &style_sets[GEANY_FILETYPES_PO].styling[6]);
	get_keyfile_style(config, config_home, "msgctxt_text", &style_sets[GEANY_FILETYPES_PO].styling[7]);
	get_keyfile_style(config, config_home, "fuzzy", &style_sets[GEANY_FILETYPES_PO].styling[8]);

	style_sets[GEANY_FILETYPES_PO].keywords = NULL;
}


static void styleset_po(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_PO;

	apply_filetype_properties(sci, SCLEX_PO, ft_id);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_PO, 0);
	set_sci_style(sci, SCE_PO_DEFAULT, GEANY_FILETYPES_PO, 0);
	set_sci_style(sci, SCE_PO_COMMENT, GEANY_FILETYPES_PO, 1);
	set_sci_style(sci, SCE_PO_MSGID, GEANY_FILETYPES_PO, 2);
	set_sci_style(sci, SCE_PO_MSGID_TEXT, GEANY_FILETYPES_PO, 3);
	set_sci_style(sci, SCE_PO_MSGSTR, GEANY_FILETYPES_PO, 4);
	set_sci_style(sci, SCE_PO_MSGSTR_TEXT, GEANY_FILETYPES_PO, 5);
	set_sci_style(sci, SCE_PO_MSGCTXT, GEANY_FILETYPES_PO, 6);
	set_sci_style(sci, SCE_PO_MSGCTXT_TEXT, GEANY_FILETYPES_PO, 7);
	set_sci_style(sci, SCE_PO_FUZZY, GEANY_FILETYPES_PO, 8);
}


static void styleset_conf_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_CONF, 6);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_CONF].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_CONF].styling[1]);
	get_keyfile_style(config, config_home, "section", &style_sets[GEANY_FILETYPES_CONF].styling[2]);
	get_keyfile_style(config, config_home, "key", &style_sets[GEANY_FILETYPES_CONF].styling[3]);
	get_keyfile_style(config, config_home, "assignment", &style_sets[GEANY_FILETYPES_CONF].styling[4]);
	get_keyfile_style(config, config_home, "defval", &style_sets[GEANY_FILETYPES_CONF].styling[5]);

	style_sets[GEANY_FILETYPES_CONF].keywords = NULL;
}


static void styleset_conf(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_CONF;

	apply_filetype_properties(sci, SCLEX_PROPERTIES, ft_id);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_CONF, 0);
	set_sci_style(sci, SCE_PROPS_DEFAULT, GEANY_FILETYPES_CONF, 0);
	set_sci_style(sci, SCE_PROPS_COMMENT, GEANY_FILETYPES_CONF, 1);
	set_sci_style(sci, SCE_PROPS_SECTION, GEANY_FILETYPES_CONF, 2);
	set_sci_style(sci, SCE_PROPS_KEY, GEANY_FILETYPES_CONF, 3);
	set_sci_style(sci, SCE_PROPS_ASSIGNMENT, GEANY_FILETYPES_CONF, 4);
	set_sci_style(sci, SCE_PROPS_DEFVAL, GEANY_FILETYPES_CONF, 5);

	SSM(sci, SCI_SETPROPERTY, (uptr_t) "lexer.props.allow.initial.spaces", (sptr_t) "0");
}


static void styleset_asm_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_ASM, 15);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_ASM].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_ASM].styling[1]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_ASM].styling[2]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_ASM].styling[3]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_ASM].styling[4]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_ASM].styling[5]);
	get_keyfile_style(config, config_home, "cpuinstruction", &style_sets[GEANY_FILETYPES_ASM].styling[6]);
	get_keyfile_style(config, config_home, "mathinstruction", &style_sets[GEANY_FILETYPES_ASM].styling[7]);
	get_keyfile_style(config, config_home, "register", &style_sets[GEANY_FILETYPES_ASM].styling[8]);
	get_keyfile_style(config, config_home, "directive", &style_sets[GEANY_FILETYPES_ASM].styling[9]);
	get_keyfile_style(config, config_home, "directiveoperand", &style_sets[GEANY_FILETYPES_ASM].styling[10]);
	get_keyfile_style(config, config_home, "commentblock", &style_sets[GEANY_FILETYPES_ASM].styling[11]);
	get_keyfile_style(config, config_home, "character", &style_sets[GEANY_FILETYPES_ASM].styling[12]);
	get_keyfile_style(config, config_home, "stringeol", &style_sets[GEANY_FILETYPES_ASM].styling[13]);
	get_keyfile_style(config, config_home, "extinstruction", &style_sets[GEANY_FILETYPES_ASM].styling[14]);

	style_sets[GEANY_FILETYPES_ASM].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "instructions", GEANY_FILETYPES_ASM, 0);
	get_keyfile_keywords(config, config_home, "registers", GEANY_FILETYPES_ASM, 1);
	get_keyfile_keywords(config, config_home, "directives", GEANY_FILETYPES_ASM, 2);
	style_sets[GEANY_FILETYPES_ASM].keywords[3] = NULL;
}


static void styleset_asm(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_ASM;

	apply_filetype_properties(sci, SCLEX_ASM, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_ASM].keywords[0]);
	/*SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_ASM].keywords[0]);*/
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_ASM].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 3, (sptr_t) style_sets[GEANY_FILETYPES_ASM].keywords[2]);
	/*SSM(sci, SCI_SETKEYWORDS, 5, (sptr_t) style_sets[GEANY_FILETYPES_ASM].keywords[0]);*/

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_ASM, 0);
	set_sci_style(sci, SCE_ASM_DEFAULT, GEANY_FILETYPES_ASM, 0);
	set_sci_style(sci, SCE_ASM_COMMENT, GEANY_FILETYPES_ASM, 1);
	set_sci_style(sci, SCE_ASM_NUMBER, GEANY_FILETYPES_ASM, 2);
	set_sci_style(sci, SCE_ASM_STRING, GEANY_FILETYPES_ASM, 3);
	set_sci_style(sci, SCE_ASM_OPERATOR, GEANY_FILETYPES_ASM, 4);
	set_sci_style(sci, SCE_ASM_IDENTIFIER, GEANY_FILETYPES_ASM, 5);
	set_sci_style(sci, SCE_ASM_CPUINSTRUCTION, GEANY_FILETYPES_ASM, 6);
	set_sci_style(sci, SCE_ASM_MATHINSTRUCTION, GEANY_FILETYPES_ASM, 7);
	set_sci_style(sci, SCE_ASM_REGISTER, GEANY_FILETYPES_ASM, 8);
	set_sci_style(sci, SCE_ASM_DIRECTIVE, GEANY_FILETYPES_ASM, 9);
	set_sci_style(sci, SCE_ASM_DIRECTIVEOPERAND, GEANY_FILETYPES_ASM, 10);
	set_sci_style(sci, SCE_ASM_COMMENTBLOCK, GEANY_FILETYPES_ASM, 11);
	set_sci_style(sci, SCE_ASM_CHARACTER, GEANY_FILETYPES_ASM, 12);
	set_sci_style(sci, SCE_ASM_STRINGEOL, GEANY_FILETYPES_ASM, 13);
	set_sci_style(sci, SCE_ASM_EXTINSTRUCTION, GEANY_FILETYPES_ASM, 14);
}


static void styleset_f77_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_F77, 15);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_F77].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_F77].styling[1]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_F77].styling[2]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_F77].styling[3]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_F77].styling[4]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_F77].styling[5]);
	get_keyfile_style(config, config_home, "string2", &style_sets[GEANY_FILETYPES_F77].styling[6]);
	get_keyfile_style(config, config_home, "word", &style_sets[GEANY_FILETYPES_F77].styling[7]);
	get_keyfile_style(config, config_home, "word2", &style_sets[GEANY_FILETYPES_F77].styling[8]);
	get_keyfile_style(config, config_home, "word3", &style_sets[GEANY_FILETYPES_F77].styling[9]);
	get_keyfile_style(config, config_home, "preprocessor", &style_sets[GEANY_FILETYPES_F77].styling[10]);
	get_keyfile_style(config, config_home, "operator2", &style_sets[GEANY_FILETYPES_F77].styling[11]);
	get_keyfile_style(config, config_home, "continuation", &style_sets[GEANY_FILETYPES_F77].styling[12]);
	get_keyfile_style(config, config_home, "stringeol", &style_sets[GEANY_FILETYPES_F77].styling[13]);
	get_keyfile_style(config, config_home, "label", &style_sets[GEANY_FILETYPES_F77].styling[14]);

	style_sets[GEANY_FILETYPES_F77].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_F77, 0);
	get_keyfile_keywords(config, config_home, "intrinsic_functions", GEANY_FILETYPES_F77, 1);
	get_keyfile_keywords(config, config_home, "user_functions", GEANY_FILETYPES_F77, 2);
	style_sets[GEANY_FILETYPES_F77].keywords[3] = NULL;
}


static void styleset_f77(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_F77;

	apply_filetype_properties(sci, SCLEX_F77, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_F77].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_F77].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_F77].keywords[2]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_F77, 0);
	set_sci_style(sci, SCE_F_DEFAULT, GEANY_FILETYPES_F77, 0);
	set_sci_style(sci, SCE_F_COMMENT, GEANY_FILETYPES_F77, 1);
	set_sci_style(sci, SCE_F_NUMBER, GEANY_FILETYPES_F77, 2);
	set_sci_style(sci, SCE_F_STRING1, GEANY_FILETYPES_F77, 3);
	set_sci_style(sci, SCE_F_OPERATOR, GEANY_FILETYPES_F77, 4);
	set_sci_style(sci, SCE_F_IDENTIFIER, GEANY_FILETYPES_F77, 5);
	set_sci_style(sci, SCE_F_STRING2, GEANY_FILETYPES_F77, 6);
	set_sci_style(sci, SCE_F_WORD, GEANY_FILETYPES_F77, 7);
	set_sci_style(sci, SCE_F_WORD2, GEANY_FILETYPES_F77, 8);
	set_sci_style(sci, SCE_F_WORD3, GEANY_FILETYPES_F77, 9);
	set_sci_style(sci, SCE_F_PREPROCESSOR, GEANY_FILETYPES_F77, 10);
	set_sci_style(sci, SCE_F_OPERATOR2, GEANY_FILETYPES_F77, 11);
	set_sci_style(sci, SCE_F_CONTINUATION, GEANY_FILETYPES_F77, 12);
	set_sci_style(sci, SCE_F_STRINGEOL, GEANY_FILETYPES_F77, 13);
	set_sci_style(sci, SCE_F_LABEL, GEANY_FILETYPES_F77, 14);
}


static void styleset_fortran_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_FORTRAN, 15);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_FORTRAN].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_FORTRAN].styling[1]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_FORTRAN].styling[2]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_FORTRAN].styling[3]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_FORTRAN].styling[4]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_FORTRAN].styling[5]);
	get_keyfile_style(config, config_home, "string2", &style_sets[GEANY_FILETYPES_FORTRAN].styling[6]);
	get_keyfile_style(config, config_home, "word", &style_sets[GEANY_FILETYPES_FORTRAN].styling[7]);
	get_keyfile_style(config, config_home, "word2", &style_sets[GEANY_FILETYPES_FORTRAN].styling[8]);
	get_keyfile_style(config, config_home, "word3", &style_sets[GEANY_FILETYPES_FORTRAN].styling[9]);
	get_keyfile_style(config, config_home, "preprocessor", &style_sets[GEANY_FILETYPES_FORTRAN].styling[10]);
	get_keyfile_style(config, config_home, "operator2", &style_sets[GEANY_FILETYPES_FORTRAN].styling[11]);
	get_keyfile_style(config, config_home, "continuation", &style_sets[GEANY_FILETYPES_FORTRAN].styling[12]);
	get_keyfile_style(config, config_home, "stringeol", &style_sets[GEANY_FILETYPES_FORTRAN].styling[13]);
	get_keyfile_style(config, config_home, "label", &style_sets[GEANY_FILETYPES_FORTRAN].styling[14]);

	style_sets[GEANY_FILETYPES_FORTRAN].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_FORTRAN, 0);
	get_keyfile_keywords(config, config_home, "intrinsic_functions", GEANY_FILETYPES_FORTRAN, 1);
	get_keyfile_keywords(config, config_home, "user_functions", GEANY_FILETYPES_FORTRAN, 2);
	style_sets[GEANY_FILETYPES_FORTRAN].keywords[3] = NULL;
}


static void styleset_fortran(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_FORTRAN;

	apply_filetype_properties(sci, SCLEX_FORTRAN, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_FORTRAN].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_FORTRAN].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_FORTRAN].keywords[2]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_FORTRAN, 0);
	set_sci_style(sci, SCE_F_DEFAULT, GEANY_FILETYPES_FORTRAN, 0);
	set_sci_style(sci, SCE_F_COMMENT, GEANY_FILETYPES_FORTRAN, 1);
	set_sci_style(sci, SCE_F_NUMBER, GEANY_FILETYPES_FORTRAN, 2);
	set_sci_style(sci, SCE_F_STRING1, GEANY_FILETYPES_FORTRAN, 3);
	set_sci_style(sci, SCE_F_OPERATOR, GEANY_FILETYPES_FORTRAN, 4);
	set_sci_style(sci, SCE_F_IDENTIFIER, GEANY_FILETYPES_FORTRAN, 5);
	set_sci_style(sci, SCE_F_STRING2, GEANY_FILETYPES_FORTRAN, 6);
	set_sci_style(sci, SCE_F_WORD, GEANY_FILETYPES_FORTRAN, 7);
	set_sci_style(sci, SCE_F_WORD2, GEANY_FILETYPES_FORTRAN, 8);
	set_sci_style(sci, SCE_F_WORD3, GEANY_FILETYPES_FORTRAN, 9);
	set_sci_style(sci, SCE_F_PREPROCESSOR, GEANY_FILETYPES_FORTRAN, 10);
	set_sci_style(sci, SCE_F_OPERATOR2, GEANY_FILETYPES_FORTRAN, 11);
	set_sci_style(sci, SCE_F_CONTINUATION, GEANY_FILETYPES_FORTRAN, 12);
	set_sci_style(sci, SCE_F_STRINGEOL, GEANY_FILETYPES_FORTRAN, 13);
	set_sci_style(sci, SCE_F_LABEL, GEANY_FILETYPES_FORTRAN, 14);
}


static void styleset_sql_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_SQL, 15);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_SQL].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_SQL].styling[1]);
	get_keyfile_style(config, config_home, "commentline", &style_sets[GEANY_FILETYPES_SQL].styling[2]);
	get_keyfile_style(config, config_home, "commentdoc", &style_sets[GEANY_FILETYPES_SQL].styling[3]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_SQL].styling[4]);
	get_keyfile_style(config, config_home, "word", &style_sets[GEANY_FILETYPES_SQL].styling[5]);
	get_keyfile_style(config, config_home, "word2", &style_sets[GEANY_FILETYPES_SQL].styling[6]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_SQL].styling[7]);
	get_keyfile_style(config, config_home, "character", &style_sets[GEANY_FILETYPES_SQL].styling[8]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_SQL].styling[9]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_SQL].styling[10]);
	get_keyfile_style(config, config_home, "sqlplus", &style_sets[GEANY_FILETYPES_SQL].styling[11]);
	get_keyfile_style(config, config_home, "sqlplus_prompt", &style_sets[GEANY_FILETYPES_SQL].styling[12]);
	get_keyfile_style(config, config_home, "sqlplus_comment", &style_sets[GEANY_FILETYPES_SQL].styling[13]);
	get_keyfile_style(config, config_home, "quotedidentifier", &style_sets[GEANY_FILETYPES_SQL].styling[14]);

	style_sets[GEANY_FILETYPES_SQL].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_SQL, 0);
	style_sets[GEANY_FILETYPES_SQL].keywords[1] = NULL;
}


static void styleset_sql(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_SQL;

	apply_filetype_properties(sci, SCLEX_SQL, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_SQL].keywords[0]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_SQL, 0);
	set_sci_style(sci, SCE_SQL_DEFAULT, GEANY_FILETYPES_SQL, 0);
	set_sci_style(sci, SCE_SQL_COMMENT, GEANY_FILETYPES_SQL, 1);
	set_sci_style(sci, SCE_SQL_COMMENTLINE, GEANY_FILETYPES_SQL, 2);
	set_sci_style(sci, SCE_SQL_COMMENTDOC, GEANY_FILETYPES_SQL, 3);
	set_sci_style(sci, SCE_SQL_NUMBER, GEANY_FILETYPES_SQL, 4);
	set_sci_style(sci, SCE_SQL_WORD, GEANY_FILETYPES_SQL, 5);
	set_sci_style(sci, SCE_SQL_WORD2, GEANY_FILETYPES_SQL, 6);
	set_sci_style(sci, SCE_SQL_STRING, GEANY_FILETYPES_SQL, 7);
	set_sci_style(sci, SCE_SQL_CHARACTER, GEANY_FILETYPES_SQL, 8);
	set_sci_style(sci, SCE_SQL_OPERATOR, GEANY_FILETYPES_SQL, 9);
	set_sci_style(sci, SCE_SQL_IDENTIFIER, GEANY_FILETYPES_SQL, 10);
	set_sci_style(sci, SCE_SQL_SQLPLUS, GEANY_FILETYPES_SQL, 11);
	set_sci_style(sci, SCE_SQL_SQLPLUS_PROMPT, GEANY_FILETYPES_SQL, 12);
	set_sci_style(sci, SCE_SQL_SQLPLUS_COMMENT, GEANY_FILETYPES_SQL, 13);
	set_sci_style(sci, SCE_SQL_QUOTEDIDENTIFIER, GEANY_FILETYPES_SQL, 14);
}


static void styleset_markdown_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_MARKDOWN, 17);

	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[0]);
	get_keyfile_style(config, config_home, "strong", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[1]);
	get_keyfile_style(config, config_home, "emphasis", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[2]);
	get_keyfile_style(config, config_home, "header1", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[3]);
	get_keyfile_style(config, config_home, "header2", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[4]);
	get_keyfile_style(config, config_home, "header3", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[5]);
	get_keyfile_style(config, config_home, "header4", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[6]);
	get_keyfile_style(config, config_home, "header5", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[7]);
	get_keyfile_style(config, config_home, "header6", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[8]);
	get_keyfile_style(config, config_home, "ulist_item", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[9]);
	get_keyfile_style(config, config_home, "olist_item", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[10]);
	get_keyfile_style(config, config_home, "blockquote", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[11]);
	get_keyfile_style(config, config_home, "strikeout", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[12]);
	get_keyfile_style(config, config_home, "hrule", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[13]);
	get_keyfile_style(config, config_home, "link", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[14]);
	get_keyfile_style(config, config_home, "code", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[15]);
	get_keyfile_style(config, config_home, "codebk", &style_sets[GEANY_FILETYPES_MARKDOWN].styling[16]);

	style_sets[GEANY_FILETYPES_MARKDOWN].keywords = NULL;
}


static void styleset_markdown(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_MARKDOWN;

	apply_filetype_properties(sci, SCLEX_MARKDOWN, ft_id);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_MARKDOWN, 0);
	set_sci_style(sci, SCE_MARKDOWN_DEFAULT, GEANY_FILETYPES_MARKDOWN, 0);
	set_sci_style(sci, SCE_MARKDOWN_LINE_BEGIN, GEANY_FILETYPES_MARKDOWN, 0);
	set_sci_style(sci, SCE_MARKDOWN_PRECHAR, GEANY_FILETYPES_MARKDOWN, 0);
	set_sci_style(sci, SCE_MARKDOWN_STRONG1, GEANY_FILETYPES_MARKDOWN, 1);
	set_sci_style(sci, SCE_MARKDOWN_STRONG2, GEANY_FILETYPES_MARKDOWN, 1);
	set_sci_style(sci, SCE_MARKDOWN_EM1, GEANY_FILETYPES_MARKDOWN, 2);
	set_sci_style(sci, SCE_MARKDOWN_EM2, GEANY_FILETYPES_MARKDOWN, 2);
	set_sci_style(sci, SCE_MARKDOWN_HEADER1, GEANY_FILETYPES_MARKDOWN, 3);
	set_sci_style(sci, SCE_MARKDOWN_HEADER2, GEANY_FILETYPES_MARKDOWN, 4);
	set_sci_style(sci, SCE_MARKDOWN_HEADER3, GEANY_FILETYPES_MARKDOWN, 5);
	set_sci_style(sci, SCE_MARKDOWN_HEADER4, GEANY_FILETYPES_MARKDOWN, 6);
	set_sci_style(sci, SCE_MARKDOWN_HEADER5, GEANY_FILETYPES_MARKDOWN, 7);
	set_sci_style(sci, SCE_MARKDOWN_HEADER6, GEANY_FILETYPES_MARKDOWN, 8);
	set_sci_style(sci, SCE_MARKDOWN_ULIST_ITEM, GEANY_FILETYPES_MARKDOWN, 9);
	set_sci_style(sci, SCE_MARKDOWN_OLIST_ITEM, GEANY_FILETYPES_MARKDOWN, 10);
	set_sci_style(sci, SCE_MARKDOWN_BLOCKQUOTE, GEANY_FILETYPES_MARKDOWN, 11);
	set_sci_style(sci, SCE_MARKDOWN_STRIKEOUT, GEANY_FILETYPES_MARKDOWN, 12);
	set_sci_style(sci, SCE_MARKDOWN_HRULE, GEANY_FILETYPES_MARKDOWN, 13);
	set_sci_style(sci, SCE_MARKDOWN_LINK, GEANY_FILETYPES_MARKDOWN, 14);
	set_sci_style(sci, SCE_MARKDOWN_CODE, GEANY_FILETYPES_MARKDOWN, 15);
	set_sci_style(sci, SCE_MARKDOWN_CODE2, GEANY_FILETYPES_MARKDOWN, 15);
	set_sci_style(sci, SCE_MARKDOWN_CODEBK, GEANY_FILETYPES_MARKDOWN, 16);
}


static void styleset_haskell_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_HASKELL, 17);

	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_HASKELL].styling[0]);
	get_keyfile_style(config, config_home, "commentline", &style_sets[GEANY_FILETYPES_HASKELL].styling[1]);
	get_keyfile_style(config, config_home, "commentblock", &style_sets[GEANY_FILETYPES_HASKELL].styling[2]);
	get_keyfile_style(config, config_home, "commentblock2", &style_sets[GEANY_FILETYPES_HASKELL].styling[3]);
	get_keyfile_style(config, config_home, "commentblock3", &style_sets[GEANY_FILETYPES_HASKELL].styling[4]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_HASKELL].styling[5]);
	get_keyfile_style(config, config_home, "keyword", &style_sets[GEANY_FILETYPES_HASKELL].styling[6]);
	get_keyfile_style(config, config_home, "import", &style_sets[GEANY_FILETYPES_HASKELL].styling[7]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_HASKELL].styling[8]);
	get_keyfile_style(config, config_home, "character", &style_sets[GEANY_FILETYPES_HASKELL].styling[9]);
	get_keyfile_style(config, config_home, "class", &style_sets[GEANY_FILETYPES_HASKELL].styling[10]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_HASKELL].styling[11]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_HASKELL].styling[12]);
	get_keyfile_style(config, config_home, "instance", &style_sets[GEANY_FILETYPES_HASKELL].styling[13]);
	get_keyfile_style(config, config_home, "capital", &style_sets[GEANY_FILETYPES_HASKELL].styling[14]);
	get_keyfile_style(config, config_home, "module", &style_sets[GEANY_FILETYPES_HASKELL].styling[15]);
	get_keyfile_style(config, config_home, "data", &style_sets[GEANY_FILETYPES_HASKELL].styling[16]);

	style_sets[GEANY_FILETYPES_HASKELL].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_HASKELL, 0);
	style_sets[GEANY_FILETYPES_HASKELL].keywords[1] = NULL;
}


static void styleset_haskell(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_HASKELL;

	apply_filetype_properties(sci, SCLEX_HASKELL, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_HASKELL].keywords[0]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_HASKELL, 0);
	set_sci_style(sci, SCE_HA_DEFAULT, GEANY_FILETYPES_HASKELL, 0);
	set_sci_style(sci, SCE_HA_COMMENTLINE, GEANY_FILETYPES_HASKELL, 1);
	set_sci_style(sci, SCE_HA_COMMENTBLOCK, GEANY_FILETYPES_HASKELL, 2);
	set_sci_style(sci, SCE_HA_COMMENTBLOCK2, GEANY_FILETYPES_HASKELL, 3);
	set_sci_style(sci, SCE_HA_COMMENTBLOCK3, GEANY_FILETYPES_HASKELL, 4);
	set_sci_style(sci, SCE_HA_NUMBER, GEANY_FILETYPES_HASKELL, 5);
	set_sci_style(sci, SCE_HA_KEYWORD, GEANY_FILETYPES_HASKELL, 6);
	set_sci_style(sci, SCE_HA_IMPORT, GEANY_FILETYPES_HASKELL, 7);
	set_sci_style(sci, SCE_HA_STRING, GEANY_FILETYPES_HASKELL, 8);
	set_sci_style(sci, SCE_HA_CHARACTER, GEANY_FILETYPES_HASKELL, 9);
	set_sci_style(sci, SCE_HA_CLASS, GEANY_FILETYPES_HASKELL, 10);
	set_sci_style(sci, SCE_HA_OPERATOR, GEANY_FILETYPES_HASKELL, 11);
	set_sci_style(sci, SCE_HA_IDENTIFIER, GEANY_FILETYPES_HASKELL, 12);
	set_sci_style(sci, SCE_HA_INSTANCE, GEANY_FILETYPES_HASKELL, 13);
	set_sci_style(sci, SCE_HA_CAPITAL, GEANY_FILETYPES_HASKELL, 14);
	set_sci_style(sci, SCE_HA_MODULE, GEANY_FILETYPES_HASKELL, 15);
	set_sci_style(sci, SCE_HA_DATA, GEANY_FILETYPES_HASKELL, 16);
}


static void styleset_caml_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_CAML, 14);

	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_CAML].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_CAML].styling[1]);
	get_keyfile_style(config, config_home, "comment1", &style_sets[GEANY_FILETYPES_CAML].styling[2]);
	get_keyfile_style(config, config_home, "comment2", &style_sets[GEANY_FILETYPES_CAML].styling[3]);
	get_keyfile_style(config, config_home, "comment3", &style_sets[GEANY_FILETYPES_CAML].styling[4]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_CAML].styling[5]);
	get_keyfile_style(config, config_home, "keyword", &style_sets[GEANY_FILETYPES_CAML].styling[6]);
	get_keyfile_style(config, config_home, "keyword2", &style_sets[GEANY_FILETYPES_CAML].styling[7]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_CAML].styling[8]);
	get_keyfile_style(config, config_home, "char", &style_sets[GEANY_FILETYPES_CAML].styling[9]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_CAML].styling[10]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_CAML].styling[11]);
	get_keyfile_style(config, config_home, "tagname", &style_sets[GEANY_FILETYPES_CAML].styling[12]);
	get_keyfile_style(config, config_home, "linenum", &style_sets[GEANY_FILETYPES_CAML].styling[13]);

	style_sets[GEANY_FILETYPES_CAML].keywords = g_new(gchar*, 3);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_CAML, 0);
	get_keyfile_keywords(config, config_home, "keywords_optional", GEANY_FILETYPES_CAML, 1);
	style_sets[GEANY_FILETYPES_CAML].keywords[2] = NULL;
}


static void styleset_caml(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_CAML;

	apply_filetype_properties(sci, SCLEX_CAML, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_CAML].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_CAML].keywords[1]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_CAML, 0);
	set_sci_style(sci, SCE_CAML_DEFAULT, GEANY_FILETYPES_CAML, 0);
	set_sci_style(sci, SCE_CAML_COMMENT, GEANY_FILETYPES_CAML, 1);
	set_sci_style(sci, SCE_CAML_COMMENT1, GEANY_FILETYPES_CAML, 2);
	set_sci_style(sci, SCE_CAML_COMMENT2, GEANY_FILETYPES_CAML, 3);
	set_sci_style(sci, SCE_CAML_COMMENT3, GEANY_FILETYPES_CAML, 4);
	set_sci_style(sci, SCE_CAML_NUMBER, GEANY_FILETYPES_CAML, 5);
	set_sci_style(sci, SCE_CAML_KEYWORD, GEANY_FILETYPES_CAML, 6);
	set_sci_style(sci, SCE_CAML_KEYWORD2, GEANY_FILETYPES_CAML, 7);
	set_sci_style(sci, SCE_CAML_STRING, GEANY_FILETYPES_CAML, 8);
	set_sci_style(sci, SCE_CAML_CHAR, GEANY_FILETYPES_CAML, 9);
	set_sci_style(sci, SCE_CAML_OPERATOR, GEANY_FILETYPES_CAML, 10);
	set_sci_style(sci, SCE_CAML_IDENTIFIER, GEANY_FILETYPES_CAML, 11);
	set_sci_style(sci, SCE_CAML_TAGNAME, GEANY_FILETYPES_CAML, 12);
	set_sci_style(sci, SCE_CAML_LINENUM, GEANY_FILETYPES_CAML, 13);
}


static void styleset_tcl_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_TCL, 16);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_TCL].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_TCL].styling[1]);
	get_keyfile_style(config, config_home, "commentline", &style_sets[GEANY_FILETYPES_TCL].styling[2]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_TCL].styling[3]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_TCL].styling[4]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_TCL].styling[5]);
	get_keyfile_style(config, config_home, "wordinquote", &style_sets[GEANY_FILETYPES_TCL].styling[6]);
	get_keyfile_style(config, config_home, "inquote", &style_sets[GEANY_FILETYPES_TCL].styling[7]);
	get_keyfile_style(config, config_home, "substitution", &style_sets[GEANY_FILETYPES_TCL].styling[8]);
	get_keyfile_style(config, config_home, "modifier", &style_sets[GEANY_FILETYPES_TCL].styling[9]);
	get_keyfile_style(config, config_home, "expand", &style_sets[GEANY_FILETYPES_TCL].styling[10]);
	get_keyfile_style(config, config_home, "wordtcl", &style_sets[GEANY_FILETYPES_TCL].styling[11]);
	get_keyfile_style(config, config_home, "wordtk", &style_sets[GEANY_FILETYPES_TCL].styling[12]);
	get_keyfile_style(config, config_home, "worditcl", &style_sets[GEANY_FILETYPES_TCL].styling[13]);
	get_keyfile_style(config, config_home, "wordtkcmds", &style_sets[GEANY_FILETYPES_TCL].styling[14]);
	get_keyfile_style(config, config_home, "wordexpand", &style_sets[GEANY_FILETYPES_TCL].styling[15]);

	style_sets[GEANY_FILETYPES_TCL].keywords = g_new(gchar*, 6);
	get_keyfile_keywords(config, config_home, "tcl", GEANY_FILETYPES_TCL, 0);
	get_keyfile_keywords(config, config_home, "tk", GEANY_FILETYPES_TCL, 1);
	get_keyfile_keywords(config, config_home, "itcl", GEANY_FILETYPES_TCL, 2);
	get_keyfile_keywords(config, config_home, "tkcommands", GEANY_FILETYPES_TCL, 3);
	get_keyfile_keywords(config, config_home, "expand", GEANY_FILETYPES_TCL, 4);
	style_sets[GEANY_FILETYPES_TCL].keywords[5] = NULL;
}


static void styleset_tcl(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_TCL;

	apply_filetype_properties(sci, SCLEX_TCL, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_TCL].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_TCL].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_TCL].keywords[2]);
	SSM(sci, SCI_SETKEYWORDS, 3, (sptr_t) style_sets[GEANY_FILETYPES_TCL].keywords[3]);
	SSM(sci, SCI_SETKEYWORDS, 4, (sptr_t) style_sets[GEANY_FILETYPES_TCL].keywords[4]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_TCL, 0);
	set_sci_style(sci, SCE_TCL_DEFAULT, GEANY_FILETYPES_TCL, 0);
	set_sci_style(sci, SCE_TCL_COMMENT, GEANY_FILETYPES_TCL, 1);
	set_sci_style(sci, SCE_TCL_COMMENTLINE, GEANY_FILETYPES_TCL, 2);
	set_sci_style(sci, SCE_TCL_NUMBER, GEANY_FILETYPES_TCL, 3);
	set_sci_style(sci, SCE_TCL_OPERATOR, GEANY_FILETYPES_TCL, 4);
	set_sci_style(sci, SCE_TCL_IDENTIFIER, GEANY_FILETYPES_TCL, 5);
	set_sci_style(sci, SCE_TCL_WORD_IN_QUOTE, GEANY_FILETYPES_TCL, 6);
	set_sci_style(sci, SCE_TCL_IN_QUOTE, GEANY_FILETYPES_TCL, 7);
	set_sci_style(sci, SCE_TCL_SUBSTITUTION, GEANY_FILETYPES_TCL, 8);
	set_sci_style(sci, SCE_TCL_MODIFIER, GEANY_FILETYPES_TCL, 9);
	set_sci_style(sci, SCE_TCL_EXPAND, GEANY_FILETYPES_TCL, 10);
	set_sci_style(sci, SCE_TCL_WORD, GEANY_FILETYPES_TCL, 11);
	set_sci_style(sci, SCE_TCL_WORD2, GEANY_FILETYPES_TCL, 12);
	set_sci_style(sci, SCE_TCL_WORD3, GEANY_FILETYPES_TCL, 13);
	set_sci_style(sci, SCE_TCL_WORD4, GEANY_FILETYPES_TCL, 14);
	set_sci_style(sci, SCE_TCL_WORD5, GEANY_FILETYPES_TCL, 15);
}


static void styleset_matlab_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_MATLAB, 9);
	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_MATLAB].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_MATLAB].styling[1]);
	get_keyfile_style(config, config_home, "command", &style_sets[GEANY_FILETYPES_MATLAB].styling[2]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_MATLAB].styling[3]);
	get_keyfile_style(config, config_home, "keyword", &style_sets[GEANY_FILETYPES_MATLAB].styling[4]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_MATLAB].styling[5]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_MATLAB].styling[6]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_MATLAB].styling[7]);
	get_keyfile_style(config, config_home, "doublequotedstring", &style_sets[GEANY_FILETYPES_MATLAB].styling[8]);

	style_sets[GEANY_FILETYPES_MATLAB].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_MATLAB, 0);
	style_sets[GEANY_FILETYPES_MATLAB].keywords[1] = NULL;
}


static void styleset_matlab(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_MATLAB;

	apply_filetype_properties(sci, SCLEX_MATLAB, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_MATLAB].keywords[0]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_MATLAB, 0);
	set_sci_style(sci, SCE_MATLAB_DEFAULT, GEANY_FILETYPES_MATLAB, 0);
	set_sci_style(sci, SCE_MATLAB_COMMENT, GEANY_FILETYPES_MATLAB, 1);
	set_sci_style(sci, SCE_MATLAB_COMMAND, GEANY_FILETYPES_MATLAB, 2);
	set_sci_style(sci, SCE_MATLAB_NUMBER, GEANY_FILETYPES_MATLAB, 3);
	set_sci_style(sci, SCE_MATLAB_KEYWORD, GEANY_FILETYPES_MATLAB, 4);
	set_sci_style(sci, SCE_MATLAB_STRING, GEANY_FILETYPES_MATLAB, 5);
	set_sci_style(sci, SCE_MATLAB_OPERATOR, GEANY_FILETYPES_MATLAB, 6);
	set_sci_style(sci, SCE_MATLAB_IDENTIFIER, GEANY_FILETYPES_MATLAB, 7);
	set_sci_style(sci, SCE_MATLAB_DOUBLEQUOTESTRING, GEANY_FILETYPES_MATLAB, 8);
}


static void styleset_d_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_D, 18);

	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_D].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_D].styling[1]);
	get_keyfile_style(config, config_home, "commentline", &style_sets[GEANY_FILETYPES_D].styling[2]);
	get_keyfile_style(config, config_home, "commentdoc", &style_sets[GEANY_FILETYPES_D].styling[3]);
	get_keyfile_style(config, config_home, "commentdocnested", &style_sets[GEANY_FILETYPES_D].styling[4]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_D].styling[5]);
	get_keyfile_style(config, config_home, "word", &style_sets[GEANY_FILETYPES_D].styling[6]);
	get_keyfile_style(config, config_home, "word2", &style_sets[GEANY_FILETYPES_D].styling[7]);
	get_keyfile_style(config, config_home, "word3", &style_sets[GEANY_FILETYPES_D].styling[8]);
	get_keyfile_style(config, config_home, "typedef", &style_sets[GEANY_FILETYPES_D].styling[9]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_D].styling[10]);
	get_keyfile_style(config, config_home, "stringeol", &style_sets[GEANY_FILETYPES_D].styling[11]);
	get_keyfile_style(config, config_home, "character", &style_sets[GEANY_FILETYPES_D].styling[12]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_D].styling[13]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_D].styling[14]);
	get_keyfile_style(config, config_home, "commentlinedoc", &style_sets[GEANY_FILETYPES_D].styling[15]);
	get_keyfile_style(config, config_home, "commentdockeyword", &style_sets[GEANY_FILETYPES_D].styling[16]);
	get_keyfile_style(config, config_home, "commentdockeyworderror", &style_sets[GEANY_FILETYPES_D].styling[17]);

	style_sets[GEANY_FILETYPES_D].keywords = g_new(gchar*, 5);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_D, 0);
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_D, 1);
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_D, 2);
	get_keyfile_keywords(config, config_home, "types", GEANY_FILETYPES_D, 3);
	style_sets[GEANY_FILETYPES_D].keywords[4] = NULL;
}


static void styleset_d(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_D;

	apply_filetype_properties(sci, SCLEX_D, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_D].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_D].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_D].keywords[2]);
	SSM(sci, SCI_SETKEYWORDS, 3, (sptr_t) style_sets[GEANY_FILETYPES_D].keywords[3]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_D, 0);
	set_sci_style(sci, SCE_D_DEFAULT, GEANY_FILETYPES_D, 0);
	set_sci_style(sci, SCE_D_COMMENT, GEANY_FILETYPES_D, 1);
	set_sci_style(sci, SCE_D_COMMENTLINE, GEANY_FILETYPES_D, 2);
	set_sci_style(sci, SCE_D_COMMENTDOC, GEANY_FILETYPES_D, 3);
	set_sci_style(sci, SCE_D_COMMENTNESTED, GEANY_FILETYPES_D, 4);
	set_sci_style(sci, SCE_D_NUMBER, GEANY_FILETYPES_D, 5);
	set_sci_style(sci, SCE_D_WORD, GEANY_FILETYPES_D, 6);
	set_sci_style(sci, SCE_D_WORD2, GEANY_FILETYPES_D, 7);
	set_sci_style(sci, SCE_D_WORD3, GEANY_FILETYPES_D, 8);
	set_sci_style(sci, SCE_D_TYPEDEF, GEANY_FILETYPES_D, 9);
	set_sci_style(sci, SCE_D_STRING, GEANY_FILETYPES_D, 10);	/* also for other strings below */
	set_sci_style(sci, SCE_D_STRINGEOL, GEANY_FILETYPES_D, 11);
	set_sci_style(sci, SCE_D_CHARACTER, GEANY_FILETYPES_D, 12);
	set_sci_style(sci, SCE_D_OPERATOR, GEANY_FILETYPES_D, 13);
	set_sci_style(sci, SCE_D_IDENTIFIER, GEANY_FILETYPES_D, 14);
	set_sci_style(sci, SCE_D_COMMENTLINEDOC, GEANY_FILETYPES_D, 15);
	set_sci_style(sci, SCE_D_COMMENTDOCKEYWORD, GEANY_FILETYPES_D, 16);
	set_sci_style(sci, SCE_D_COMMENTDOCKEYWORDERROR, GEANY_FILETYPES_D, 17);

	/* copy existing styles */
	set_sci_style(sci, SCE_D_STRINGB, GEANY_FILETYPES_D, 10);	/* `string` */
	set_sci_style(sci, SCE_D_STRINGR, GEANY_FILETYPES_D, 10);	/* r"string" */
}


static void styleset_ferite_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_FERITE, 20);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_FERITE);

	style_sets[GEANY_FILETYPES_FERITE].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_FERITE, 0);
	get_keyfile_keywords(config, config_home, "types", GEANY_FILETYPES_FERITE, 1);
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_FERITE, 2);
	style_sets[GEANY_FILETYPES_FERITE].keywords[3] = NULL;
}


static void styleset_ferite(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_FERITE;

	apply_filetype_properties(sci, SCLEX_CPP, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_FERITE].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_FERITE].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_FERITE].keywords[2]);

	styleset_c_like(sci, GEANY_FILETYPES_FERITE);
}


static void styleset_vhdl_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_VHDL, 15);

	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_VHDL].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_VHDL].styling[1]);
	get_keyfile_style(config, config_home, "comment_line_bang", &style_sets[GEANY_FILETYPES_VHDL].styling[2]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_VHDL].styling[3]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_VHDL].styling[4]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_VHDL].styling[5]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_VHDL].styling[6]);
	get_keyfile_style(config, config_home, "stringeol", &style_sets[GEANY_FILETYPES_VHDL].styling[7]);
	get_keyfile_style(config, config_home, "keyword", &style_sets[GEANY_FILETYPES_VHDL].styling[8]);
	get_keyfile_style(config, config_home, "stdoperator", &style_sets[GEANY_FILETYPES_VHDL].styling[9]);
	get_keyfile_style(config, config_home, "attribute", &style_sets[GEANY_FILETYPES_VHDL].styling[10]);
	get_keyfile_style(config, config_home, "stdfunction", &style_sets[GEANY_FILETYPES_VHDL].styling[11]);
	get_keyfile_style(config, config_home, "stdpackage", &style_sets[GEANY_FILETYPES_VHDL].styling[12]);
	get_keyfile_style(config, config_home, "stdtype", &style_sets[GEANY_FILETYPES_VHDL].styling[13]);
	get_keyfile_style(config, config_home, "userword", &style_sets[GEANY_FILETYPES_VHDL].styling[14]);

	style_sets[GEANY_FILETYPES_VHDL].keywords = g_new(gchar*, 8);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_VHDL, 0);
	get_keyfile_keywords(config, config_home, "operators", GEANY_FILETYPES_VHDL, 1);
	get_keyfile_keywords(config, config_home, "attributes", GEANY_FILETYPES_VHDL, 2);
	get_keyfile_keywords(config, config_home, "std_functions", GEANY_FILETYPES_VHDL, 3);
	get_keyfile_keywords(config, config_home, "std_packages", GEANY_FILETYPES_VHDL, 4);
	get_keyfile_keywords(config, config_home, "std_types", GEANY_FILETYPES_VHDL, 5);
	get_keyfile_keywords(config, config_home, "userwords", GEANY_FILETYPES_VHDL, 6);
	style_sets[GEANY_FILETYPES_VHDL].keywords[7] = NULL;
}


static void styleset_vhdl(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_VHDL;

	apply_filetype_properties(sci, SCLEX_VHDL, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_VHDL].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_VHDL].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_VHDL].keywords[2]);
	SSM(sci, SCI_SETKEYWORDS, 3, (sptr_t) style_sets[GEANY_FILETYPES_VHDL].keywords[3]);
	SSM(sci, SCI_SETKEYWORDS, 4, (sptr_t) style_sets[GEANY_FILETYPES_VHDL].keywords[4]);
	SSM(sci, SCI_SETKEYWORDS, 5, (sptr_t) style_sets[GEANY_FILETYPES_VHDL].keywords[5]);
	SSM(sci, SCI_SETKEYWORDS, 6, (sptr_t) style_sets[GEANY_FILETYPES_VHDL].keywords[6]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_VHDL, 0);
	set_sci_style(sci, SCE_VHDL_DEFAULT, GEANY_FILETYPES_VHDL, 0);
	set_sci_style(sci, SCE_VHDL_COMMENT, GEANY_FILETYPES_VHDL, 1);
	set_sci_style(sci, SCE_VHDL_COMMENTLINEBANG, GEANY_FILETYPES_VHDL, 2);
	set_sci_style(sci, SCE_VHDL_NUMBER, GEANY_FILETYPES_VHDL, 3);
	set_sci_style(sci, SCE_VHDL_STRING, GEANY_FILETYPES_VHDL, 4);
	set_sci_style(sci, SCE_VHDL_OPERATOR, GEANY_FILETYPES_VHDL, 5);
	set_sci_style(sci, SCE_VHDL_IDENTIFIER, GEANY_FILETYPES_VHDL, 6);
	set_sci_style(sci, SCE_VHDL_STRINGEOL, GEANY_FILETYPES_VHDL, 7);
	set_sci_style(sci, SCE_VHDL_KEYWORD, GEANY_FILETYPES_VHDL, 8);
	set_sci_style(sci, SCE_VHDL_STDOPERATOR, GEANY_FILETYPES_VHDL, 9);
	set_sci_style(sci, SCE_VHDL_ATTRIBUTE, GEANY_FILETYPES_VHDL, 10);
	set_sci_style(sci, SCE_VHDL_STDFUNCTION, GEANY_FILETYPES_VHDL, 11);
	set_sci_style(sci, SCE_VHDL_STDPACKAGE, GEANY_FILETYPES_VHDL, 12);
	set_sci_style(sci, SCE_VHDL_STDTYPE, GEANY_FILETYPES_VHDL, 13);
	set_sci_style(sci, SCE_VHDL_USERWORD, GEANY_FILETYPES_VHDL, 14);
}


static void styleset_yaml_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_YAML, 10);

	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_YAML].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_YAML].styling[1]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_YAML].styling[2]);
	get_keyfile_style(config, config_home, "keyword", &style_sets[GEANY_FILETYPES_YAML].styling[3]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_YAML].styling[4]);
	get_keyfile_style(config, config_home, "reference", &style_sets[GEANY_FILETYPES_YAML].styling[5]);
	get_keyfile_style(config, config_home, "document", &style_sets[GEANY_FILETYPES_YAML].styling[6]);
	get_keyfile_style(config, config_home, "text", &style_sets[GEANY_FILETYPES_YAML].styling[7]);
	get_keyfile_style(config, config_home, "error", &style_sets[GEANY_FILETYPES_YAML].styling[8]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_YAML].styling[9]);

	style_sets[GEANY_FILETYPES_YAML].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_YAML, 0);
	style_sets[GEANY_FILETYPES_YAML].keywords[1] = NULL;
}


static void styleset_yaml(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_YAML;

	apply_filetype_properties(sci, SCLEX_YAML, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_YAML].keywords[0]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_YAML, 0);
	set_sci_style(sci, SCE_YAML_DEFAULT, GEANY_FILETYPES_YAML, 0);
	set_sci_style(sci, SCE_YAML_COMMENT, GEANY_FILETYPES_YAML, 1);
	set_sci_style(sci, SCE_YAML_IDENTIFIER, GEANY_FILETYPES_YAML, 2);
	set_sci_style(sci, SCE_YAML_KEYWORD, GEANY_FILETYPES_YAML, 3);
	set_sci_style(sci, SCE_YAML_NUMBER, GEANY_FILETYPES_YAML, 4);
	set_sci_style(sci, SCE_YAML_REFERENCE, GEANY_FILETYPES_YAML, 5);
	set_sci_style(sci, SCE_YAML_DOCUMENT, GEANY_FILETYPES_YAML, 6);
	set_sci_style(sci, SCE_YAML_TEXT, GEANY_FILETYPES_YAML, 7);
	set_sci_style(sci, SCE_YAML_ERROR, GEANY_FILETYPES_YAML, 8);
	set_sci_style(sci, SCE_YAML_OPERATOR, GEANY_FILETYPES_YAML, 9);
}


static void styleset_js_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_JS, 20);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_JS);

	style_sets[GEANY_FILETYPES_JS].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_JS, 0);
	style_sets[GEANY_FILETYPES_JS].keywords[1] = NULL;
}


static void styleset_js(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_JS;

	apply_filetype_properties(sci, SCLEX_CPP, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_JS].keywords[0]);

	styleset_c_like(sci, GEANY_FILETYPES_JS);
}


static void styleset_lua_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_LUA, 20);

	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_LUA].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_LUA].styling[1]);
	get_keyfile_style(config, config_home, "commentline", &style_sets[GEANY_FILETYPES_LUA].styling[2]);
	get_keyfile_style(config, config_home, "commentdoc", &style_sets[GEANY_FILETYPES_LUA].styling[3]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_LUA].styling[4]);
	get_keyfile_style(config, config_home, "word", &style_sets[GEANY_FILETYPES_LUA].styling[5]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_LUA].styling[6]);
	get_keyfile_style(config, config_home, "character", &style_sets[GEANY_FILETYPES_LUA].styling[7]);
	get_keyfile_style(config, config_home, "literalstring", &style_sets[GEANY_FILETYPES_LUA].styling[8]);
	get_keyfile_style(config, config_home, "preprocessor", &style_sets[GEANY_FILETYPES_LUA].styling[9]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_LUA].styling[10]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_LUA].styling[11]);
	get_keyfile_style(config, config_home, "stringeol", &style_sets[GEANY_FILETYPES_LUA].styling[12]);
	get_keyfile_style(config, config_home, "function_basic", &style_sets[GEANY_FILETYPES_LUA].styling[13]);
	get_keyfile_style(config, config_home, "function_other", &style_sets[GEANY_FILETYPES_LUA].styling[14]);
	get_keyfile_style(config, config_home, "coroutines", &style_sets[GEANY_FILETYPES_LUA].styling[15]);
	get_keyfile_style(config, config_home, "word5", &style_sets[GEANY_FILETYPES_LUA].styling[16]);
	get_keyfile_style(config, config_home, "word6", &style_sets[GEANY_FILETYPES_LUA].styling[17]);
	get_keyfile_style(config, config_home, "word7", &style_sets[GEANY_FILETYPES_LUA].styling[18]);
	get_keyfile_style(config, config_home, "word8", &style_sets[GEANY_FILETYPES_LUA].styling[19]);

	style_sets[GEANY_FILETYPES_LUA].keywords = g_new(gchar*, 9);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_LUA, 0);
	get_keyfile_keywords(config, config_home, "function_basic", GEANY_FILETYPES_LUA, 1);
	get_keyfile_keywords(config, config_home, "function_other", GEANY_FILETYPES_LUA, 2);
	get_keyfile_keywords(config, config_home, "coroutines", GEANY_FILETYPES_LUA, 3);
	get_keyfile_keywords(config, config_home, "user1", GEANY_FILETYPES_LUA, 4);
	get_keyfile_keywords(config, config_home, "user2", GEANY_FILETYPES_LUA, 5);
	get_keyfile_keywords(config, config_home, "user3", GEANY_FILETYPES_LUA, 6);
	get_keyfile_keywords(config, config_home, "user4", GEANY_FILETYPES_LUA, 7);
	style_sets[GEANY_FILETYPES_LUA].keywords[8] = NULL;
}


static void styleset_lua(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_LUA;

	apply_filetype_properties(sci, SCLEX_LUA, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_LUA].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_LUA].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_LUA].keywords[2]);
	SSM(sci, SCI_SETKEYWORDS, 3, (sptr_t) style_sets[GEANY_FILETYPES_LUA].keywords[3]);
	SSM(sci, SCI_SETKEYWORDS, 4, (sptr_t) style_sets[GEANY_FILETYPES_LUA].keywords[4]);
	SSM(sci, SCI_SETKEYWORDS, 5, (sptr_t) style_sets[GEANY_FILETYPES_LUA].keywords[5]);
	SSM(sci, SCI_SETKEYWORDS, 6, (sptr_t) style_sets[GEANY_FILETYPES_LUA].keywords[6]);
	SSM(sci, SCI_SETKEYWORDS, 7, (sptr_t) style_sets[GEANY_FILETYPES_LUA].keywords[7]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_LUA, 0);
	set_sci_style(sci, SCE_LUA_DEFAULT, GEANY_FILETYPES_LUA, 0);
	set_sci_style(sci, SCE_LUA_COMMENT, GEANY_FILETYPES_LUA, 1);
	set_sci_style(sci, SCE_LUA_COMMENTLINE, GEANY_FILETYPES_LUA, 2);
	set_sci_style(sci, SCE_LUA_COMMENTDOC, GEANY_FILETYPES_LUA, 3);
	set_sci_style(sci, SCE_LUA_NUMBER, GEANY_FILETYPES_LUA, 4);
	set_sci_style(sci, SCE_LUA_WORD, GEANY_FILETYPES_LUA, 5);
	set_sci_style(sci, SCE_LUA_STRING, GEANY_FILETYPES_LUA, 6);
	set_sci_style(sci, SCE_LUA_CHARACTER, GEANY_FILETYPES_LUA, 7);
	set_sci_style(sci, SCE_LUA_LITERALSTRING, GEANY_FILETYPES_LUA, 8);
	set_sci_style(sci, SCE_LUA_PREPROCESSOR, GEANY_FILETYPES_LUA, 9);
	set_sci_style(sci, SCE_LUA_OPERATOR, GEANY_FILETYPES_LUA, 10);
	set_sci_style(sci, SCE_LUA_IDENTIFIER, GEANY_FILETYPES_LUA, 11);
	set_sci_style(sci, SCE_LUA_STRINGEOL, GEANY_FILETYPES_LUA, 12);
	set_sci_style(sci, SCE_LUA_WORD2, GEANY_FILETYPES_LUA, 13);
	set_sci_style(sci, SCE_LUA_WORD3, GEANY_FILETYPES_LUA, 14);
	set_sci_style(sci, SCE_LUA_WORD4, GEANY_FILETYPES_LUA, 15);
	set_sci_style(sci, SCE_LUA_WORD5, GEANY_FILETYPES_LUA, 16);
	set_sci_style(sci, SCE_LUA_WORD6, GEANY_FILETYPES_LUA, 17);
	set_sci_style(sci, SCE_LUA_WORD7, GEANY_FILETYPES_LUA, 18);
	set_sci_style(sci, SCE_LUA_WORD8, GEANY_FILETYPES_LUA, 19);
}


static void styleset_basic_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_BASIC, 19);

	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_BASIC].styling[0]);
	get_keyfile_style(config, config_home, "comment", &style_sets[GEANY_FILETYPES_BASIC].styling[1]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_BASIC].styling[2]);
	get_keyfile_style(config, config_home, "word", &style_sets[GEANY_FILETYPES_BASIC].styling[3]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_BASIC].styling[4]);
	get_keyfile_style(config, config_home, "preprocessor", &style_sets[GEANY_FILETYPES_BASIC].styling[5]);
	get_keyfile_style(config, config_home, "operator", &style_sets[GEANY_FILETYPES_BASIC].styling[6]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_BASIC].styling[7]);
	get_keyfile_style(config, config_home, "date", &style_sets[GEANY_FILETYPES_BASIC].styling[8]);
	get_keyfile_style(config, config_home, "stringeol", &style_sets[GEANY_FILETYPES_BASIC].styling[9]);
	get_keyfile_style(config, config_home, "word2", &style_sets[GEANY_FILETYPES_BASIC].styling[10]);
	get_keyfile_style(config, config_home, "word3", &style_sets[GEANY_FILETYPES_BASIC].styling[11]);
	get_keyfile_style(config, config_home, "word4", &style_sets[GEANY_FILETYPES_BASIC].styling[12]);
	get_keyfile_style(config, config_home, "constant", &style_sets[GEANY_FILETYPES_BASIC].styling[13]);
	get_keyfile_style(config, config_home, "asm", &style_sets[GEANY_FILETYPES_BASIC].styling[14]);
	get_keyfile_style(config, config_home, "label", &style_sets[GEANY_FILETYPES_BASIC].styling[15]);
	get_keyfile_style(config, config_home, "error", &style_sets[GEANY_FILETYPES_BASIC].styling[16]);
	get_keyfile_style(config, config_home, "hexnumber", &style_sets[GEANY_FILETYPES_BASIC].styling[17]);
	get_keyfile_style(config, config_home, "binnumber", &style_sets[GEANY_FILETYPES_BASIC].styling[18]);

	style_sets[GEANY_FILETYPES_BASIC].keywords = g_new(gchar*, 5);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_BASIC, 0);
	get_keyfile_keywords(config, config_home, "preprocessor", GEANY_FILETYPES_BASIC, 1);
	get_keyfile_keywords(config, config_home, "user1", GEANY_FILETYPES_BASIC, 2);
	get_keyfile_keywords(config, config_home, "user2", GEANY_FILETYPES_BASIC, 3);
	style_sets[GEANY_FILETYPES_BASIC].keywords[4] = NULL;
}


static void styleset_basic(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_BASIC;

	apply_filetype_properties(sci, SCLEX_FREEBASIC, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_BASIC].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_BASIC].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 2, (sptr_t) style_sets[GEANY_FILETYPES_BASIC].keywords[2]);
	SSM(sci, SCI_SETKEYWORDS, 3, (sptr_t) style_sets[GEANY_FILETYPES_BASIC].keywords[3]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_BASIC, 0);
	set_sci_style(sci, SCE_B_DEFAULT, GEANY_FILETYPES_BASIC, 0);
	set_sci_style(sci, SCE_B_COMMENT, GEANY_FILETYPES_BASIC, 1);
	set_sci_style(sci, SCE_B_NUMBER, GEANY_FILETYPES_BASIC, 2);
	set_sci_style(sci, SCE_B_KEYWORD, GEANY_FILETYPES_BASIC, 3);
	set_sci_style(sci, SCE_B_STRING, GEANY_FILETYPES_BASIC, 4);
	set_sci_style(sci, SCE_B_PREPROCESSOR, GEANY_FILETYPES_BASIC, 5);
	set_sci_style(sci, SCE_B_OPERATOR, GEANY_FILETYPES_BASIC, 6);
	set_sci_style(sci, SCE_B_IDENTIFIER, GEANY_FILETYPES_BASIC, 7);
	set_sci_style(sci, SCE_B_DATE, GEANY_FILETYPES_BASIC, 8);
	set_sci_style(sci, SCE_B_STRINGEOL, GEANY_FILETYPES_BASIC, 9);
	set_sci_style(sci, SCE_B_KEYWORD2, GEANY_FILETYPES_BASIC, 10);
	set_sci_style(sci, SCE_B_KEYWORD3, GEANY_FILETYPES_BASIC, 11);
	set_sci_style(sci, SCE_B_KEYWORD4, GEANY_FILETYPES_BASIC, 12);
	set_sci_style(sci, SCE_B_CONSTANT, GEANY_FILETYPES_BASIC, 13);
	set_sci_style(sci, SCE_B_ASM, GEANY_FILETYPES_BASIC, 14); /* (still?) unused by the lexer */
	set_sci_style(sci, SCE_B_LABEL, GEANY_FILETYPES_BASIC, 15);
	set_sci_style(sci, SCE_B_ERROR, GEANY_FILETYPES_BASIC, 16);
	set_sci_style(sci, SCE_B_HEXNUMBER, GEANY_FILETYPES_BASIC, 17);
	set_sci_style(sci, SCE_B_BINNUMBER, GEANY_FILETYPES_BASIC, 18);
}


static void styleset_actionscript_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_AS, 20);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_AS);

	style_sets[GEANY_FILETYPES_AS].keywords = g_new(gchar *, 4);

	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_AS, 0);
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_AS, 1);
	get_keyfile_keywords(config, config_home, "classes", GEANY_FILETYPES_AS, 2);
	style_sets[GEANY_FILETYPES_AS].keywords[3] = NULL;
}


static void styleset_actionscript(ScintillaObject *sci)
{
	apply_filetype_properties(sci, SCLEX_CPP, GEANY_FILETYPES_AS);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_AS].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_AS].keywords[2]);
	SSM(sci, SCI_SETKEYWORDS, 3, (sptr_t) style_sets[GEANY_FILETYPES_AS].keywords[1]);

	styleset_c_like(sci, GEANY_FILETYPES_AS);
}


static void styleset_haxe_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_HAXE, 20);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_HAXE);

	style_sets[GEANY_FILETYPES_HAXE].keywords = g_new(gchar*, 4);

	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_HAXE, 0);
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_HAXE, 1);
	get_keyfile_keywords(config, config_home, "classes", GEANY_FILETYPES_HAXE, 2);

	style_sets[GEANY_FILETYPES_HAXE].keywords[3] = NULL;
}


static void styleset_haxe(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_HAXE;

	apply_filetype_properties(sci, SCLEX_CPP, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_HAXE].keywords[0]);
	SSM(sci, SCI_SETKEYWORDS, 1, (sptr_t) style_sets[GEANY_FILETYPES_HAXE].keywords[1]);
	SSM(sci, SCI_SETKEYWORDS, 3, (sptr_t) style_sets[GEANY_FILETYPES_HAXE].keywords[2]);

	styleset_c_like(sci, GEANY_FILETYPES_HAXE);
}


static void styleset_ada_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_styleset(GEANY_FILETYPES_ADA, 12);

	get_keyfile_style(config, config_home, "default", &style_sets[GEANY_FILETYPES_ADA].styling[0]);
	get_keyfile_style(config, config_home, "word", &style_sets[GEANY_FILETYPES_ADA].styling[1]);
	get_keyfile_style(config, config_home, "identifier", &style_sets[GEANY_FILETYPES_ADA].styling[2]);
	get_keyfile_style(config, config_home, "number", &style_sets[GEANY_FILETYPES_ADA].styling[3]);
	get_keyfile_style(config, config_home, "delimiter", &style_sets[GEANY_FILETYPES_ADA].styling[4]);
	get_keyfile_style(config, config_home, "character", &style_sets[GEANY_FILETYPES_ADA].styling[5]);
	get_keyfile_style(config, config_home, "charactereol", &style_sets[GEANY_FILETYPES_ADA].styling[6]);
	get_keyfile_style(config, config_home, "string", &style_sets[GEANY_FILETYPES_ADA].styling[7]);
	get_keyfile_style(config, config_home, "stringeol", &style_sets[GEANY_FILETYPES_ADA].styling[8]);
	get_keyfile_style(config, config_home, "label", &style_sets[GEANY_FILETYPES_ADA].styling[9]);
	get_keyfile_style(config, config_home, "commentline", &style_sets[GEANY_FILETYPES_ADA].styling[10]);
	get_keyfile_style(config, config_home, "illegal", &style_sets[GEANY_FILETYPES_ADA].styling[11]);

	style_sets[GEANY_FILETYPES_ADA].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_ADA, 0);
	style_sets[GEANY_FILETYPES_ADA].keywords[1] = NULL;
}


static void styleset_ada(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_ADA;

	apply_filetype_properties(sci, SCLEX_ADA, ft_id);

	SSM(sci, SCI_SETKEYWORDS, 0, (sptr_t) style_sets[GEANY_FILETYPES_ADA].keywords[0]);

	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_ADA, 0);
	set_sci_style(sci, SCE_ADA_DEFAULT, GEANY_FILETYPES_ADA, 0);
	set_sci_style(sci, SCE_ADA_WORD, GEANY_FILETYPES_ADA, 1);
	set_sci_style(sci, SCE_ADA_IDENTIFIER, GEANY_FILETYPES_ADA, 2);
	set_sci_style(sci, SCE_ADA_NUMBER, GEANY_FILETYPES_ADA, 3);
	set_sci_style(sci, SCE_ADA_DELIMITER, GEANY_FILETYPES_ADA, 4);
	set_sci_style(sci, SCE_ADA_CHARACTER, GEANY_FILETYPES_ADA, 5);
	set_sci_style(sci, SCE_ADA_CHARACTEREOL, GEANY_FILETYPES_ADA, 6);
	set_sci_style(sci, SCE_ADA_STRING, GEANY_FILETYPES_ADA, 7);
	set_sci_style(sci, SCE_ADA_STRINGEOL, GEANY_FILETYPES_ADA, 8);
	set_sci_style(sci, SCE_ADA_LABEL, GEANY_FILETYPES_ADA, 9);
	set_sci_style(sci, SCE_ADA_COMMENTLINE, GEANY_FILETYPES_ADA, 10);
	set_sci_style(sci, SCE_ADA_ILLEGAL, GEANY_FILETYPES_ADA, 11);
}


/* lang_name is the name used for the styleset_foo_init function, e.g. foo. */
#define init_styleset_case(ft_id, init_styleset_func) \
	case (ft_id): \
		init_styleset_func(filetype_idx, config, configh); \
		break

/* Called by filetypes_load_config(). */
void highlighting_init_styles(gint filetype_idx, GKeyFile *config, GKeyFile *configh)
{
	if (!style_sets)
		style_sets = g_new0(StyleSet, filetypes_array->len);

	/* Clear old information if necessary - e.g. when reloading config */
	free_styleset(filetype_idx);

	/* All stylesets depend on filetypes.common */
	if (filetype_idx != GEANY_FILETYPES_NONE)
		filetypes_load_config(GEANY_FILETYPES_NONE, FALSE);

	switch (filetype_idx)
	{
		init_styleset_case(GEANY_FILETYPES_NONE,	styleset_common_init);
		init_styleset_case(GEANY_FILETYPES_ADA,		styleset_ada_init);
		init_styleset_case(GEANY_FILETYPES_ASM,		styleset_asm_init);
		init_styleset_case(GEANY_FILETYPES_BASIC,	styleset_basic_init);
		init_styleset_case(GEANY_FILETYPES_C,		styleset_c_init);
		init_styleset_case(GEANY_FILETYPES_CAML,	styleset_caml_init);
		init_styleset_case(GEANY_FILETYPES_CMAKE,	styleset_cmake_init);
		init_styleset_case(GEANY_FILETYPES_CONF,	styleset_conf_init);
		init_styleset_case(GEANY_FILETYPES_CPP,		styleset_cpp_init);
		init_styleset_case(GEANY_FILETYPES_CS,		styleset_cs_init);
		init_styleset_case(GEANY_FILETYPES_CSS,		styleset_css_init);
		init_styleset_case(GEANY_FILETYPES_D,		styleset_d_init);
		init_styleset_case(GEANY_FILETYPES_DIFF,	styleset_diff_init);
		init_styleset_case(GEANY_FILETYPES_DOCBOOK,	styleset_docbook_init);
		init_styleset_case(GEANY_FILETYPES_FERITE,	styleset_ferite_init);
		init_styleset_case(GEANY_FILETYPES_F77,		styleset_f77_init);
		init_styleset_case(GEANY_FILETYPES_FORTRAN,	styleset_fortran_init);
		init_styleset_case(GEANY_FILETYPES_GLSL,	styleset_glsl_init);
		init_styleset_case(GEANY_FILETYPES_HASKELL,	styleset_haskell_init);
		init_styleset_case(GEANY_FILETYPES_HAXE,	styleset_haxe_init);
		init_styleset_case(GEANY_FILETYPES_AS,		styleset_actionscript_init);
		init_styleset_case(GEANY_FILETYPES_HTML,	styleset_html_init);
		init_styleset_case(GEANY_FILETYPES_JAVA,	styleset_java_init);
		init_styleset_case(GEANY_FILETYPES_JS,		styleset_js_init);
		init_styleset_case(GEANY_FILETYPES_LATEX,	styleset_latex_init);
		init_styleset_case(GEANY_FILETYPES_LUA,		styleset_lua_init);
		init_styleset_case(GEANY_FILETYPES_MAKE,	styleset_makefile_init);
		init_styleset_case(GEANY_FILETYPES_MATLAB,	styleset_matlab_init);
		init_styleset_case(GEANY_FILETYPES_MARKDOWN,	styleset_markdown_init);
		init_styleset_case(GEANY_FILETYPES_NSIS,	styleset_nsis_init);
		init_styleset_case(GEANY_FILETYPES_PASCAL,	styleset_pascal_init);
		init_styleset_case(GEANY_FILETYPES_PERL,	styleset_perl_init);
		init_styleset_case(GEANY_FILETYPES_PHP,		styleset_php_init);
		init_styleset_case(GEANY_FILETYPES_PO,		styleset_po_init);
		init_styleset_case(GEANY_FILETYPES_PYTHON,	styleset_python_init);
		init_styleset_case(GEANY_FILETYPES_R,		styleset_r_init);
		init_styleset_case(GEANY_FILETYPES_RUBY,	styleset_ruby_init);
		init_styleset_case(GEANY_FILETYPES_SH,		styleset_sh_init);
		init_styleset_case(GEANY_FILETYPES_SQL,		styleset_sql_init);
		init_styleset_case(GEANY_FILETYPES_TCL,		styleset_tcl_init);
		init_styleset_case(GEANY_FILETYPES_VALA,	styleset_vala_init);
		init_styleset_case(GEANY_FILETYPES_VHDL,	styleset_vhdl_init);
		init_styleset_case(GEANY_FILETYPES_XML,		styleset_markup_init);
		init_styleset_case(GEANY_FILETYPES_YAML,	styleset_yaml_init);
	}
	/* should be done in filetypes.c really: */
	if (filetype_idx != GEANY_FILETYPES_NONE)
		get_keyfile_wordchars(config, configh, &style_sets[filetype_idx].wordchars);
}


/* lang_name is the name used for the styleset_foo function, e.g. foo. */
#define styleset_case(ft_id, styleset_func) \
	case (ft_id): \
		styleset_func (sci); \
		break

void highlighting_set_styles(ScintillaObject *sci, gint filetype_idx)
{
	filetypes_load_config(filetype_idx, FALSE);	/* load filetypes.ext */

	/* load tags files (some lexers highlight global typenames) */
	if (filetype_idx != GEANY_FILETYPES_NONE)
		symbols_global_tags_loaded(filetype_idx);

	switch (filetype_idx)
	{
		styleset_case(GEANY_FILETYPES_ADA,		styleset_ada);
		styleset_case(GEANY_FILETYPES_ASM,		styleset_asm);
		styleset_case(GEANY_FILETYPES_BASIC,	styleset_basic);
		styleset_case(GEANY_FILETYPES_C,		styleset_c);
		styleset_case(GEANY_FILETYPES_CAML,		styleset_caml);
		styleset_case(GEANY_FILETYPES_CMAKE,	styleset_cmake);
		styleset_case(GEANY_FILETYPES_CONF,		styleset_conf);
		styleset_case(GEANY_FILETYPES_CPP,		styleset_cpp);
		styleset_case(GEANY_FILETYPES_CS,		styleset_cs);
		styleset_case(GEANY_FILETYPES_CSS,		styleset_css);
		styleset_case(GEANY_FILETYPES_D,		styleset_d);
		styleset_case(GEANY_FILETYPES_DIFF,		styleset_diff);
		styleset_case(GEANY_FILETYPES_DOCBOOK,	styleset_docbook);
		styleset_case(GEANY_FILETYPES_FERITE,	styleset_ferite);
		styleset_case(GEANY_FILETYPES_F77,		styleset_f77);
		styleset_case(GEANY_FILETYPES_FORTRAN,	styleset_fortran);
		styleset_case(GEANY_FILETYPES_GLSL,		styleset_glsl);
		styleset_case(GEANY_FILETYPES_HASKELL,	styleset_haskell);
		styleset_case(GEANY_FILETYPES_HAXE,		styleset_haxe);
		styleset_case(GEANY_FILETYPES_AS,		styleset_actionscript);
		styleset_case(GEANY_FILETYPES_HTML,		styleset_html);
		styleset_case(GEANY_FILETYPES_JAVA,		styleset_java);
		styleset_case(GEANY_FILETYPES_JS,		styleset_js);
		styleset_case(GEANY_FILETYPES_LATEX,	styleset_latex);
		styleset_case(GEANY_FILETYPES_LUA,		styleset_lua);
		styleset_case(GEANY_FILETYPES_MAKE,		styleset_makefile);
		styleset_case(GEANY_FILETYPES_MARKDOWN,	styleset_markdown);
		styleset_case(GEANY_FILETYPES_MATLAB,	styleset_matlab);
		styleset_case(GEANY_FILETYPES_NSIS,		styleset_nsis);
		styleset_case(GEANY_FILETYPES_PASCAL,	styleset_pascal);
		styleset_case(GEANY_FILETYPES_PERL,		styleset_perl);
		styleset_case(GEANY_FILETYPES_PHP,		styleset_php);
		styleset_case(GEANY_FILETYPES_PO,		styleset_po);
		styleset_case(GEANY_FILETYPES_PYTHON,	styleset_python);
		styleset_case(GEANY_FILETYPES_R,		styleset_r);
		styleset_case(GEANY_FILETYPES_RUBY,		styleset_ruby);
		styleset_case(GEANY_FILETYPES_SH,		styleset_sh);
		styleset_case(GEANY_FILETYPES_SQL,		styleset_sql);
		styleset_case(GEANY_FILETYPES_TCL,		styleset_tcl);
		styleset_case(GEANY_FILETYPES_VALA,		styleset_vala);
		styleset_case(GEANY_FILETYPES_VHDL,		styleset_vhdl);
		styleset_case(GEANY_FILETYPES_XML,		styleset_xml);
		styleset_case(GEANY_FILETYPES_YAML,		styleset_yaml);
		case GEANY_FILETYPES_NONE:
		default:
			styleset_default(sci, filetype_idx);
	}
}


/** Retrieve a style @a style_id for the filetype @a ft_id.
 * If the style was not already initialised
 * (e.g. by by opening a file of this type), it will be initialised. The returned pointer is
 * owned by Geany and must not be freed.
 * @param ft_id Filetype ID, e.g. @c GEANY_FILETYPES_DIFF.
 * @param style_id A Scintilla lexer style, e.g. @c SCE_DIFF_ADDED. See scintilla/include/SciLexer.h.
 * @return A pointer to the style struct.
 * @see Scintilla messages @c SCI_STYLEGETFORE, etc, for use with ScintillaFuncs::send_message(). */
const GeanyLexerStyle *highlighting_get_style(gint ft_id, gint style_id)
{
	if (ft_id < 0 || ft_id >= (gint)filetypes_array->len)
		return NULL;

	/* ensure filetype loaded */
	filetypes_load_config(ft_id, FALSE);

	/* TODO: style_id might not be the real array index (Scintilla styles are not always synced
	 * with array indices) */
	return get_style(ft_id, style_id);
}
