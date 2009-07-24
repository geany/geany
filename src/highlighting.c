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
static StyleSet style_sets[GEANY_MAX_BUILT_IN_FILETYPES] = {{0, NULL, NULL, NULL}};


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

typedef struct
{
	/* can take values 1 or 2 (or 3) */
	guint marker:2;
	guint lines:2;
	guint draw_line:3;
} FoldingStyle;

static struct
{
	GeanyLexerStyle	 styling[GCS_MAX];
	FoldingStyle	 folding_style;
	gboolean		 invert_all;
	gchar			*wordchars;
} common_style_set;


/* used for default styles */
typedef struct
{
	gchar			*name;
	gchar			*named_style;
	GeanyLexerStyle	*style;
} StyleEntry;


/* For filetypes.common [named_styles] section.
 * 0xBBGGRR format.
 * e.g. "comment" => &GeanyLexerStyle{0x0000d0, 0xffffff, FALSE, FALSE} */
static GHashTable *named_style_hash = NULL;

/* 0xBBGGRR format. */
static GeanyLexerStyle gsd_default = {0x000000, 0xffffff, FALSE, FALSE};


static void new_style_array(gint file_type_id, gint styling_count)
{
	StyleSet *set = &style_sets[file_type_id];

	set->count = styling_count;
	set->styling = g_new0(GeanyLexerStyle, styling_count);
}


static void styleset_free(gint file_type_id)
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
				const gchar *key, gint ft_id, gint pos, const gchar *default_value)
{
	const gchar section[] = "keywords";
	gchar *result;

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
		const gchar *key_name, const GeanyLexerStyle *default_style, GeanyLexerStyle *style)
{
	gchar **list;
	gsize len;

	g_return_if_fail(config);
	g_return_if_fail(configh);
	g_return_if_fail(key_name);
	g_return_if_fail(default_style);
	g_return_if_fail(style);

	list = g_key_file_get_string_list(configh, "styling", key_name, &len, NULL);
	if (list == NULL)
		list = g_key_file_get_string_list(config, "styling", key_name, &len, NULL);

	parse_keyfile_style(list, default_style, style);
	g_strfreev(list);
}


static void get_keyfile_named_style(GKeyFile *config, GKeyFile *configh,
		const gchar *key_name, gchar *named_style, GeanyLexerStyle *style)
{
	GeanyLexerStyle def;

	read_named_style(named_style, &def);
	get_keyfile_style(config, configh, key_name, &def, style);
}


/* Convert 0xRRGGBB to 0xBBGGRR, which scintilla expects. */
static gint rotate_rgb(gint color)
{
	return ((color & 0xFF0000) >> 16) +
		(color & 0x00FF00) +
		((color & 0x0000FF) << 16);
}


/* Convert a hard-coded style to scintilla format, allowing -1 to use the default color. */
static void convert_hex_style(GeanyLexerStyle *style)
{

	if (style->foreground == -1)
		style->foreground = gsd_default.foreground;
	else
		style->foreground = rotate_rgb(style->foreground);

	if (style->background == -1)
		style->background = gsd_default.background;
	else
		style->background = rotate_rgb(style->background);
}


/* foreground and background are in 0xRRGGBB format */
static void get_keyfile_hex(GKeyFile *config, GKeyFile *configh,
				const gchar *key,
				gint foreground, gint background, gboolean bold,
				GeanyLexerStyle *style)
{
	GeanyLexerStyle def = {foreground, background, bold, FALSE};

	convert_hex_style(&def);
	get_keyfile_style(config, configh, key, &def, style);
}


/* FIXME: is not safe for badly formed key e.g. "key=" */
static void get_keyfile_int(GKeyFile *config, GKeyFile *configh, const gchar *section,
							const gchar *key, gint fdefault_val, gint sdefault_val,
							GeanyLexerStyle *style)
{
	gchar **list;
	gchar *end1, *end2;
	gsize len;

	g_return_if_fail(config);
	g_return_if_fail(configh);
	g_return_if_fail(section);
	g_return_if_fail(key);

	list = g_key_file_get_string_list(configh, section, key, &len, NULL);
	if (list == NULL)
		list = g_key_file_get_string_list(config, section, key, &len, NULL);

	if (G_LIKELY(list != NULL) && G_LIKELY(list[0] != NULL) )
		style->foreground = strtol(list[0], &end1, 10);
	else
		style->foreground = fdefault_val;
	if (G_LIKELY(list != NULL) && G_LIKELY(list[1] != NULL) )
		style->background = strtol(list[1], &end2, 10);
	else
		style->background = sdefault_val;

	/* if there was an error, strtol() returns 0 and end is list[x], so then we use default_val */
	if (G_UNLIKELY(list == NULL) || G_UNLIKELY(list[0] == end1))
		style->foreground = fdefault_val;
	if (G_UNLIKELY(list == NULL) || G_UNLIKELY(list[1] == end2))
		style->background = sdefault_val;

	g_strfreev(list);
}


static guint invert(guint icolour)
{
	if (common_style_set.invert_all)
		return utils_invert_color(icolour);

	return icolour;
}


static GeanyLexerStyle *get_style(guint ft_id, guint styling_index)
{
	g_assert(ft_id < GEANY_MAX_BUILT_IN_FILETYPES);

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
	gint i;

	for (i = 0; i < GEANY_MAX_BUILT_IN_FILETYPES; i++)
		styleset_free(i);

	if (named_style_hash)
		g_hash_table_destroy(named_style_hash);
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

	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &common_style_set.styling[GCS_DEFAULT]);
	get_keyfile_hex(config, config_home, "selection", 0xc0c0c0, 0x7f0000, FALSE, &common_style_set.styling[GCS_SELECTION]);
	get_keyfile_hex(config, config_home, "brace_good", 0x000000, 0xffffff, FALSE, &common_style_set.styling[GCS_BRACE_GOOD]);
	get_keyfile_hex(config, config_home, "brace_bad", 0xff0000, 0xffffff, FALSE, &common_style_set.styling[GCS_BRACE_BAD]);
	get_keyfile_hex(config, config_home, "margin_linenumber", 0x000000, 0xd0d0d0, FALSE, &common_style_set.styling[GCS_MARGIN_LINENUMBER]);
	get_keyfile_hex(config, config_home, "margin_folding", 0x000000, 0xdfdfdf, FALSE, &common_style_set.styling[GCS_MARGIN_FOLDING]);
	get_keyfile_hex(config, config_home, "current_line", 0x000000, 0xe5e5e5, TRUE, &common_style_set.styling[GCS_CURRENT_LINE]);
	get_keyfile_hex(config, config_home, "caret", 0x000000, 0x000000, FALSE, &common_style_set.styling[GCS_CARET]);
	get_keyfile_hex(config, config_home, "indent_guide", 0xc0c0c0, 0xffffff, FALSE, &common_style_set.styling[GCS_INDENT_GUIDE]);
	get_keyfile_hex(config, config_home, "white_space", 0xc0c0c0, 0xffffff, TRUE, &common_style_set.styling[GCS_WHITE_SPACE]);
	get_keyfile_hex(config, config_home, "marker_line", 0x000000, 0xffff00, FALSE, &common_style_set.styling[GCS_MARKER_LINE]);
	get_keyfile_hex(config, config_home, "marker_search", 0x000000, 0x00007f, FALSE, &common_style_set.styling[GCS_MARKER_SEARCH]);
	get_keyfile_hex(config, config_home, "marker_mark", 0x000000, 0xb8f4b8, FALSE, &common_style_set.styling[GCS_MARKER_MARK]);
	{
		/* hack because get_keyfile_int uses a Style struct */
		GeanyLexerStyle tmp_style;
		get_keyfile_int(config, config_home, "styling", "folding_style",
			1, 1, &tmp_style);
		common_style_set.folding_style.marker = tmp_style.foreground;
		common_style_set.folding_style.lines = tmp_style.background;
		get_keyfile_int(config, config_home, "styling", "invert_all",
			0, 0, &tmp_style);
		common_style_set.invert_all = tmp_style.foreground;
		get_keyfile_int(config, config_home, "styling", "folding_horiz_line",
			2, 0, &tmp_style);
		common_style_set.folding_style.draw_line = tmp_style.foreground;
		get_keyfile_int(config, config_home, "styling", "caret_width",
			1, 0, &tmp_style);
		common_style_set.styling[GCS_CARET].background = tmp_style.foreground;
		get_keyfile_int(config, config_home, "styling", "line_wrap_visuals",
			3, 0, &tmp_style);
		common_style_set.styling[GCS_LINE_WRAP_VISUALS].foreground = tmp_style.foreground;
		common_style_set.styling[GCS_LINE_WRAP_VISUALS].background = tmp_style.background;
		get_keyfile_int(config, config_home, "styling", "line_wrap_indent",
			0, 0, &tmp_style);
		common_style_set.styling[GCS_LINE_WRAP_INDENT].foreground = tmp_style.foreground;
		common_style_set.styling[GCS_LINE_WRAP_INDENT].background = tmp_style.background;
		get_keyfile_int(config, config_home, "styling", "translucency",
			256, 256, &tmp_style);
		common_style_set.styling[GCS_TRANSLUCENCY].foreground = tmp_style.foreground;
		common_style_set.styling[GCS_TRANSLUCENCY].background = tmp_style.background;
		get_keyfile_int(config, config_home, "styling", "marker_translucency",
			256, 256, &tmp_style);
		common_style_set.styling[GCS_MARKER_TRANSLUCENCY].foreground = tmp_style.foreground;
		common_style_set.styling[GCS_MARKER_TRANSLUCENCY].background = tmp_style.background;
		get_keyfile_int(config, config_home, "styling", "line_height",
			0, 0, &tmp_style);
		common_style_set.styling[GCS_LINE_HEIGHT].foreground = tmp_style.foreground;
		common_style_set.styling[GCS_LINE_HEIGHT].background = tmp_style.background;
	}

	common_style_set.invert_all = interface_prefs.highlighting_invert_all =
		(common_style_set.invert_all || interface_prefs.highlighting_invert_all);
	get_keyfile_wordchars(config, config_home, &common_style_set.wordchars);
	whitespace_chars = get_keyfile_whitespace_chars(config, config_home);
}


static void styleset_common(ScintillaObject *sci)
{
	SSM(sci, SCI_STYLECLEARALL, 0, 0);

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
	switch (common_style_set.folding_style.draw_line)
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
	switch (common_style_set.folding_style.marker)
	{
		case 2:
		{
			SSM(sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEROPEN, SC_MARK_CIRCLEMINUS);
			SSM(sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDER, SC_MARK_CIRCLEPLUS);
			SSM(sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEREND, SC_MARK_CIRCLEPLUSCONNECTED);
			SSM(sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEROPENMID, SC_MARK_CIRCLEMINUSCONNECTED);
			break;
		}
		default:
		{
			SSM(sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
			SSM(sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
			SSM(sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED);
			SSM(sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED);
			break;
		}
	}

	/* choose the folding style - straight or curved, I prefer straight, so it is default ;-) */
	switch (common_style_set.folding_style.lines)
	{
		case 2:
		{
			SSM(sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNERCURVE);
			SSM(sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNERCURVE);
			break;
		}
		default:
		{
			SSM(sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER);
			SSM(sci,SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER);
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

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) style_sets[ft_id].wordchars);
	/* have to set whitespace after setting wordchars */
	SSM(sci, SCI_SETWHITESPACECHARS, 0, (sptr_t) whitespace_chars);

	SSM(sci, SCI_AUTOCSETMAXHEIGHT, editor_prefs.symbolcompletion_max_height, 0);

	styleset_common(sci);
}


#define foreach_range(i, size) \
		for (i = 0; i < size; i++)

/* entries contains the default styles for the filetype.
 * If used, entries[n].style should have foreground and background in 0xRRGGBB format, or -1. */
static void load_style_entries(GKeyFile *config, GKeyFile *config_home, gint filetype_idx,
		StyleEntry *entries, gsize entries_len)
{
	guint i;

	foreach_range(i, entries_len)
	{
		StyleEntry *entry = &entries[i];
		GeanyLexerStyle *style = &style_sets[filetype_idx].styling[i];

		if (entry->named_style)
			get_keyfile_named_style(config, config_home, entry->name, entry->named_style, style);
		else
		if (entry->style)
		{
			GeanyLexerStyle *def = entry->style;

			convert_hex_style(def);
			get_keyfile_style(config, config_home, entry->name, def, style);
		}
	}
}


/* call new_style_array(filetype_idx, >= 20) before using this. */
static void
styleset_c_like_init(GKeyFile *config, GKeyFile *config_home, gint filetype_idx)
{
	static GeanyLexerStyle uuid = {0x404080, -1, FALSE, FALSE};
	static GeanyLexerStyle verbatim = {0x301010, -1, FALSE, FALSE};
	static GeanyLexerStyle regex = {0x105090, -1, FALSE, FALSE};
	StyleEntry entries[] =
 	{
		{"default",		"default", NULL},
		{"comment",		"comment", NULL},
		{"commentline",	"comment", NULL},
		{"commentdoc",	"commentdoc", NULL},
		{"number",		"number", NULL},
		{"word",		"word", NULL},
		{"word2",		"word2", NULL},
		{"string",		"string", NULL},
		{"character",	"string", NULL},
		{"uuid",		NULL, &uuid},
		{"preprocessor","preprocessor", NULL},
		{"operator",	"operator", NULL},
		{"identifier",	"default", NULL},
		{"stringeol",	"stringeol", NULL},
		{"verbatim",	NULL, &verbatim},
		{"regex",		NULL, &regex},
		{"commentlinedoc",	"commentdoc,bold", NULL},
		{"commentdockeyword",	"commentdoc,bold,italic", NULL},
		{"commentdockeyworderror",	"commentdoc", NULL},
		{"globalclass",	"type", NULL}
	};

	load_style_entries(config, config_home, filetype_idx, entries, G_N_ELEMENTS(entries));
}


static void styleset_c_like(ScintillaObject *sci, gint filetype_idx)
{
	set_sci_style(sci, STYLE_DEFAULT, filetype_idx, 0);
	set_sci_style(sci, SCE_C_DEFAULT, filetype_idx, 0);
	set_sci_style(sci, SCE_C_COMMENT, filetype_idx, 1);
	set_sci_style(sci, SCE_C_COMMENTLINE, filetype_idx, 2);
	set_sci_style(sci, SCE_C_COMMENTDOC, filetype_idx, 3);
	set_sci_style(sci, SCE_C_NUMBER, filetype_idx, 4);
	set_sci_style(sci, SCE_C_WORD, filetype_idx, 5);
	set_sci_style(sci, SCE_C_WORD2, filetype_idx, 6);
	set_sci_style(sci, SCE_C_STRING, filetype_idx, 7);
	set_sci_style(sci, SCE_C_CHARACTER, filetype_idx, 8);
	set_sci_style(sci, SCE_C_UUID, filetype_idx, 9);
	set_sci_style(sci, SCE_C_PREPROCESSOR, filetype_idx, 10);
	set_sci_style(sci, SCE_C_OPERATOR, filetype_idx, 11);
	set_sci_style(sci, SCE_C_IDENTIFIER, filetype_idx, 12);
	set_sci_style(sci, SCE_C_STRINGEOL, filetype_idx, 13);
	set_sci_style(sci, SCE_C_VERBATIM, filetype_idx, 14);
	set_sci_style(sci, SCE_C_REGEX, filetype_idx, 15);
	set_sci_style(sci, SCE_C_COMMENTLINEDOC, filetype_idx, 16);
	set_sci_style(sci, SCE_C_COMMENTDOCKEYWORD, filetype_idx, 17);
	set_sci_style(sci, SCE_C_COMMENTDOCKEYWORDERROR, filetype_idx, 18);
	/* is used for local structs and typedefs */
	set_sci_style(sci, SCE_C_GLOBALCLASS, filetype_idx, 19);
}


static void styleset_c_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_style_array(GEANY_FILETYPES_C, 21);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_C);
	get_keyfile_int(config, config_home, "styling", "styling_within_preprocessor",
		1, 0, &style_sets[GEANY_FILETYPES_C].styling[20]);

	style_sets[GEANY_FILETYPES_C].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_C, 0, "if const struct char int float double void long for while do case switch return");
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_C, 1, "");
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_C, 2, "TODO FIXME");
	style_sets[GEANY_FILETYPES_C].keywords[3] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_C].wordchars);
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
	new_style_array(GEANY_FILETYPES_CPP, 21);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_CPP);
	get_keyfile_int(config, config_home, "styling", "styling_within_preprocessor",
		1, 0, &style_sets[GEANY_FILETYPES_CPP].styling[20]);

	style_sets[GEANY_FILETYPES_CPP].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_CPP, 0, "and and_eq asm auto bitand bitor bool break case catch char class compl const const_cast continue default delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable namespace new not not_eq operator or or_eq private protected public register reinterpret_cast return short signed sizeof static static_cast struct switch template this throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq");
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_CPP, 1, "");
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_CPP, 2, "TODO FIXME");
	style_sets[GEANY_FILETYPES_CPP].keywords[3] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_CPP].wordchars);
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
	new_style_array(GEANY_FILETYPES_GLSL, 21);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_GLSL);
	get_keyfile_int(config, config_home, "styling", "styling_within_preprocessor",
		1, 0, &style_sets[GEANY_FILETYPES_GLSL].styling[20]);

	style_sets[GEANY_FILETYPES_GLSL].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_GLSL, 0,
			"if else switch case default for while do discard return break"
			"continue true false struct void bool int uint float vec2 vec3"
			"vec4 ivec2 ivec3 ivec4 bvec2 bvec3 bvec4 uvec2 uvec3 uvec4 mat2"
			"mat3 mat4 mat2x2 mat2x3 mat2x4 mat3x2 mat3x3 mat3x4 mat4x2 mat4x3"
			"mat4x4 sampler1D sampler2D sampler3D samplerCube sampler1DShadow"
			"sampler2DShadow sampler1DArray sampler2DArray sampler1DArrayShadow"
			"sampler2DArrayShadow isampler1D isampler2D isampler3D isamplerCube"
			"isampler1DArray isampler2DArray usampler1D usampler2D usampler3D"
			"usamplerCube usampler1DArray usampler2DArray const invariant"
			"centroid in out inout attribute uniform varying smooth flat"
			"noperspective highp mediump lowp");
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_GLSL, 1, "");
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_GLSL, 2, "TODO FIXME");
	style_sets[GEANY_FILETYPES_GLSL].keywords[3] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_GLSL].wordchars);
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
	new_style_array(GEANY_FILETYPES_CS, 21);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_CS);
	get_keyfile_int(config, config_home, "styling", "styling_within_preprocessor",
		1, 0, &style_sets[GEANY_FILETYPES_CS].styling[20]);

	style_sets[GEANY_FILETYPES_CS].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_CS, 0, "\
			abstract as base bool break byte case catch char checked class \
			const continue decimal default delegate do double else enum \
			event explicit extern false finally fixed float for foreach goto if \
			implicit in int interface internal is lock long namespace new null \
			object operator out override params private protected public \
			readonly ref return sbyte sealed short sizeof stackalloc static \
			string struct switch this throw true try typeof uint ulong \
			unchecked unsafe ushort using virtual void volatile while");
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_CS, 1, "");
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_CS, 2, "");
	style_sets[GEANY_FILETYPES_CS].keywords[3] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_CS].wordchars);
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
	new_style_array(GEANY_FILETYPES_VALA, 21);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_VALA);
	get_keyfile_int(config, config_home, "styling", "styling_within_preprocessor",
		1, 0, &style_sets[GEANY_FILETYPES_VALA].styling[20]);

	style_sets[GEANY_FILETYPES_VALA].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_VALA, 0,
			"else if switch case default break continue return for foreach in do while is "
			"try catch finally throw "
			"namespace interface class struct enum signal errordomain "
			"construct callback get set base "
			"const static var weak "
			"virtual abstract override inline extern "
			"public protected private delegate "
			"out ref "

			"lock using "

			"true false null "

			"generic new delete base this value typeof sizeof "
			"throws requires ensures "

			"void bool char uchar int uint short ushort long ulong size_t ssize_t "
			"int8 uint8 int16 uint16 int32 uint32 int64 uint64 float double unichar string "

			/* these types are not referenced by the reference manual but are in glib.vapi
			 * as simple types */
			/*"constpointer time_t "*/
			);
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_VALA, 1, "");
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_VALA, 2, "");
	style_sets[GEANY_FILETYPES_VALA].keywords[3] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_VALA].wordchars);
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
	new_style_array(GEANY_FILETYPES_PASCAL, 15);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[0]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[1]);
	get_keyfile_hex(config, config_home, "comment", 0xd00000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[2]);
	get_keyfile_hex(config, config_home, "comment2", 0x3f5fbf, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[3]);
	get_keyfile_hex(config, config_home, "commentline", 0xd00000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[4]);
	get_keyfile_hex(config, config_home, "preprocessor", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[5]);
	get_keyfile_hex(config, config_home, "preprocessor2", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[6]);
	get_keyfile_hex(config, config_home, "number", 0x007F00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[7]);
	get_keyfile_hex(config, config_home, "hexnumber", 0x007F00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[8]);
	get_keyfile_hex(config, config_home, "word", 0x111199, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_PASCAL].styling[9]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[10]);
	get_keyfile_hex(config, config_home, "stringeol", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[11]);
	get_keyfile_hex(config, config_home, "character", 0x404000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[12]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[13]);
	get_keyfile_hex(config, config_home, "asm", 0x804080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PASCAL].styling[14]);

	style_sets[GEANY_FILETYPES_PASCAL].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_PASCAL, 0,
		"asm begin byte case char class do else end for function if implementation integer \
		 interface object procedure program real repeat string then to try unit until uses var word");
	style_sets[GEANY_FILETYPES_PASCAL].keywords[1] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_PASCAL].wordchars);
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
	new_style_array(GEANY_FILETYPES_MAKE, 7);
	get_keyfile_hex(config, config_home, "default", 0x00002f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_MAKE].styling[0]);
	get_keyfile_named_style(config, config_home, "comment", "comment", &style_sets[GEANY_FILETYPES_MAKE].styling[1]);
	get_keyfile_hex(config, config_home, "preprocessor", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_MAKE].styling[2]);
	get_keyfile_hex(config, config_home, "identifier", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_MAKE].styling[3]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_MAKE].styling[4]);
	get_keyfile_hex(config, config_home, "target", 0x0000ff, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_MAKE].styling[5]);
	get_keyfile_hex(config, config_home, "ideol", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_MAKE].styling[6]);

	style_sets[GEANY_FILETYPES_MAKE].keywords = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_MAKE].wordchars);
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
	new_style_array(GEANY_FILETYPES_DIFF, 8);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DIFF].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DIFF].styling[1]);
	get_keyfile_hex(config, config_home, "command", 0x7f7f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DIFF].styling[2]);
	get_keyfile_hex(config, config_home, "header", 0x7f0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DIFF].styling[3]);
	get_keyfile_hex(config, config_home, "position", 0x00007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DIFF].styling[4]);
	get_keyfile_hex(config, config_home, "deleted", 0xff2727, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DIFF].styling[5]);
	get_keyfile_hex(config, config_home, "added", 0x34b034, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DIFF].styling[6]);
	get_keyfile_hex(config, config_home, "changed", 0x7f007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DIFF].styling[7]);

	style_sets[GEANY_FILETYPES_DIFF].keywords = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_DIFF].wordchars);
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
	new_style_array(GEANY_FILETYPES_LATEX, 5);
	get_keyfile_hex(config, config_home, "default", 0x00002f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LATEX].styling[0]);
	get_keyfile_hex(config, config_home, "command", 0xff0000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_LATEX].styling[1]);
	get_keyfile_hex(config, config_home, "tag", 0x007f7f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_LATEX].styling[2]);
	get_keyfile_hex(config, config_home, "math", 0x00007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LATEX].styling[3]);
	get_keyfile_hex(config, config_home, "comment", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LATEX].styling[4]);

	style_sets[GEANY_FILETYPES_LATEX].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_LATEX, 0, "section subsection begin item");
	style_sets[GEANY_FILETYPES_LATEX].keywords[1] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_LATEX].wordchars);
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

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_PHP].wordchars);
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

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_HTML].wordchars);
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
	new_style_array(GEANY_FILETYPES_XML, 56);
	get_keyfile_hex(config, config_home, "html_default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[0]);
	get_keyfile_hex(config, config_home, "html_tag", 0x000099, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[1]);
	get_keyfile_hex(config, config_home, "html_tagunknown", 0xff0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[2]);
	get_keyfile_hex(config, config_home, "html_attribute", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[3]);
	get_keyfile_hex(config, config_home, "html_attributeunknown", 0xff0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[4]);
	get_keyfile_hex(config, config_home, "html_number", 0x800080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[5]);
	get_keyfile_hex(config, config_home, "html_doublestring", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[6]);
	get_keyfile_hex(config, config_home, "html_singlestring", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[7]);
	get_keyfile_hex(config, config_home, "html_other", 0x800080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[8]);
	get_keyfile_hex(config, config_home, "html_comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[9]);
	get_keyfile_hex(config, config_home, "html_entity", 0x800080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[10]);
	get_keyfile_hex(config, config_home, "html_tagend", 0x000080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[11]);
	get_keyfile_hex(config, config_home, "html_xmlstart", 0x000099, 0xf0f0f0, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[12]);
	get_keyfile_hex(config, config_home, "html_xmlend", 0x000099, 0xf0f0f0, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[13]);
	get_keyfile_hex(config, config_home, "html_script", 0x000080, 0xf0f0f0, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[14]);
	get_keyfile_hex(config, config_home, "html_asp", 0x004f4f, 0xf0f0f0, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[15]);
	get_keyfile_hex(config, config_home, "html_aspat", 0x004f4f, 0xf0f0f0, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[16]);
	get_keyfile_hex(config, config_home, "html_cdata", 0x660099, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[17]);
	get_keyfile_hex(config, config_home, "html_question", 0x0000ff, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[18]);
	get_keyfile_hex(config, config_home, "html_value", 0x660099, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[19]);
	get_keyfile_hex(config, config_home, "html_xccomment", 0x660099, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[20]);

	get_keyfile_hex(config, config_home, "sgml_default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[21]);
	get_keyfile_hex(config, config_home, "sgml_comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[22]);
	get_keyfile_hex(config, config_home, "sgml_special", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[23]);
	get_keyfile_hex(config, config_home, "sgml_command", 0x111199, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_XML].styling[24]);
	get_keyfile_hex(config, config_home, "sgml_doublestring", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[25]);
	get_keyfile_hex(config, config_home, "sgml_simplestring", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[26]);
	get_keyfile_hex(config, config_home, "sgml_1st_param", 0x404080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[27]);
	get_keyfile_hex(config, config_home, "sgml_entity", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[28]);
	get_keyfile_hex(config, config_home, "sgml_block_default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[29]);
	get_keyfile_hex(config, config_home, "sgml_1st_param_comment", 0x406090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[30]);
	get_keyfile_hex(config, config_home, "sgml_error", 0xff0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[31]);

	get_keyfile_hex(config, config_home, "php_default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[32]);
	get_keyfile_hex(config, config_home, "php_simplestring", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[33]);
	get_keyfile_hex(config, config_home, "php_hstring", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[34]);
	get_keyfile_hex(config, config_home, "php_number", 0x606000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[35]);
	get_keyfile_hex(config, config_home, "php_word", 0x000099, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[36]);
	get_keyfile_hex(config, config_home, "php_variable", 0x7f0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[37]);
	get_keyfile_hex(config, config_home, "php_comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[38]);
	get_keyfile_hex(config, config_home, "php_commentline", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[39]);
	get_keyfile_hex(config, config_home, "php_operator", 0x102060, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[40]);
	get_keyfile_hex(config, config_home, "php_hstring_variable", 0x101060, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[41]);
	get_keyfile_hex(config, config_home, "php_complex_variable", 0x105010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[42]);

	get_keyfile_hex(config, config_home, "jscript_start", 0x008080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[43]);
	get_keyfile_hex(config, config_home, "jscript_default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[44]);
	get_keyfile_hex(config, config_home, "jscript_comment", 0xd00000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[45]);
	get_keyfile_hex(config, config_home, "jscript_commentline", 0xd00000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[46]);
	get_keyfile_hex(config, config_home, "jscript_commentdoc", 0x3f5fbf, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_XML].styling[47]);
	get_keyfile_hex(config, config_home, "jscript_number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[48]);
	get_keyfile_hex(config, config_home, "jscript_word", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[49]);
	get_keyfile_hex(config, config_home, "jscript_keyword", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_XML].styling[50]);
	get_keyfile_hex(config, config_home, "jscript_doublestring", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[51]);
	get_keyfile_hex(config, config_home, "jscript_singlestring", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[52]);
	get_keyfile_hex(config, config_home, "jscript_symbols", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[53]);
	get_keyfile_hex(config, config_home, "jscript_stringeol", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[54]);
	get_keyfile_hex(config, config_home, "jscript_regex", 0x105090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_XML].styling[55]);

	style_sets[GEANY_FILETYPES_XML].keywords = g_new(gchar*, 7);
	get_keyfile_keywords(config, config_home, "html", GEANY_FILETYPES_XML, 0, "a abbr acronym address applet area b base basefont bdo big blockquote body br button caption center cite code col colgroup dd del dfn dir div dl dt em embed fieldset font form frame frameset h1 h2 h3 h4 h5 h6 head hr html i iframe img input ins isindex kbd label legend li link map menu meta noframes noscript object ol optgroup option p param pre q quality s samp script select small span strike strong style sub sup table tbody td textarea tfoot th thead title tr tt u ul var xmlns leftmargin topmargin abbr accept-charset accept accesskey action align alink alt archive axis background bgcolor border cellpadding cellspacing char charoff charset checked cite class classid clear codebase codetype color cols colspan compact content coords data datafld dataformatas datapagesize datasrc datetime declare defer dir disabled enctype face for frame frameborder selected headers height href hreflang hspace http-equiv id ismap label lang language link longdesc marginwidth marginheight maxlength media framespacing method multiple name nohref noresize noshade nowrap object onblur onchange onclick ondblclick onfocus onkeydown onkeypress onkeyup onload onmousedown onmousemove onmouseover onmouseout onmouseup onreset onselect onsubmit onunload profile prompt pluginspage readonly rel rev rows rowspan rules scheme scope scrolling shape size span src standby start style summary tabindex target text title type usemap valign value valuetype version vlink vspace width text password checkbox radio submit reset file hidden image public doctype xml xml:lang");
	get_keyfile_keywords(config, config_home, "javascript", GEANY_FILETYPES_XML, 1, "abs abstract acos anchor asin atan atan2 big bold boolean break byte case catch ceil char charAt charCodeAt class concat const continue cos Date debugger default delete do double else enum escape eval exp export extends false final finally fixed float floor fontcolor fontsize for fromCharCode function goto if implements import in indexOf Infinity instanceof int interface isFinite isNaN italics join lastIndexOf length link log long Math max MAX_VALUE min MIN_VALUE NaN native NEGATIVE_INFINITY new null Number package parseFloat parseInt pop POSITIVE_INFINITY pow private protected public push random return reverse round shift short sin slice small sort splice split sqrt static strike string String sub substr substring sup super switch synchronized tan this throw throws toLowerCase toString toUpperCase transient true try typeof undefined unescape unshift valueOf var void volatile while with");
	get_keyfile_keywords(config, config_home, "vbscript", GEANY_FILETYPES_XML, 2, "and as byref byval case call const continue dim do each else elseif end error exit false for function global goto if in loop me new next not nothing on optional or private public redim rem resume select set sub then to true type while with boolean byte currency date double integer long object single string type variant");
	get_keyfile_keywords(config, config_home, "python", GEANY_FILETYPES_XML, 3, "and as assert break class continue def del elif else except exec finally for from global if import in is lambda not or pass print raise return try while with yield False None True");
	get_keyfile_keywords(config, config_home, "php", GEANY_FILETYPES_XML, 4, "abstract and array as bool boolean break case catch cfunction __class__ class clone const continue declare default die __dir__ directory do double echo else elseif empty enddeclare endfor endforeach endif endswitch endwhile eval exception exit extends false __file__ final float for foreach __function__ function goto global if implements include include_once int integer interface isset __line__ list __method__ namespace __namespace__ new null object old_function or parent php_user_filter print private protected public real require require_once resource return __sleep static stdclass string switch this throw true try unset use var __wakeup while xor");
	get_keyfile_keywords(config, config_home, "sgml", GEANY_FILETYPES_XML, 5, "ELEMENT DOCTYPE ATTLIST ENTITY NOTATION");
	style_sets[GEANY_FILETYPES_XML].keywords[6] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_XML].wordchars);
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
}


static void styleset_java_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_style_array(GEANY_FILETYPES_JAVA, 20);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_JAVA);

	style_sets[GEANY_FILETYPES_JAVA].keywords = g_new(gchar*, 5);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_JAVA, 0, "\
										abstract assert break case catch class \
										const continue default do else extends final finally for future \
										generic goto if implements import inner instanceof interface \
										native new outer package private protected public rest \
										return static super switch synchronized this throw throws \
										transient try var volatile while true false null");
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_JAVA, 1, "boolean byte char double float int long null short void");
	get_keyfile_keywords(config, config_home, "doccomment", GEANY_FILETYPES_JAVA, 2, "return param author throws");
	get_keyfile_keywords(config, config_home, "typedefs", GEANY_FILETYPES_JAVA, 3, "");
	style_sets[GEANY_FILETYPES_JAVA].keywords[4] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_JAVA].wordchars);
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
	new_style_array(GEANY_FILETYPES_PERL, 35);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[0]);
	get_keyfile_hex(config, config_home, "error", 0xff0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[1]);
	get_keyfile_named_style(config, config_home, "commentline", "comment", &style_sets[GEANY_FILETYPES_PERL].styling[2]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[3]);
	get_keyfile_hex(config, config_home, "word", 0x111199, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_PERL].styling[4]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[5]);
	get_keyfile_hex(config, config_home, "character", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[6]);
	get_keyfile_hex(config, config_home, "preprocessor", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[7]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[8]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[9]);
	get_keyfile_hex(config, config_home, "scalar", 0x7f0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[10]);
	get_keyfile_hex(config, config_home, "pod", 0x035650, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[11]);
	get_keyfile_hex(config, config_home, "regex", 0x105090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[12]);
	get_keyfile_hex(config, config_home, "array", 0x105090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[13]);
	get_keyfile_hex(config, config_home, "hash", 0x105090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[14]);
	get_keyfile_hex(config, config_home, "symboltable", 0x105090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[15]);
	get_keyfile_hex(config, config_home, "backticks", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[16]);
	get_keyfile_hex(config, config_home, "pod_verbatim", 0x004000, 0xc0ffc0, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[17]);
	get_keyfile_hex(config, config_home, "reg_subst", 0x000000, 0xf0e080, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[18]);
	get_keyfile_hex(config, config_home, "datasection", 0x600000, 0xfff0d8, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[19]);
	get_keyfile_hex(config, config_home, "here_delim", 0x000000, 0xddd0dd, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[20]);
	get_keyfile_hex(config, config_home, "here_q", 0x7f007f, 0xddd0dd, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[21]);
	get_keyfile_hex(config, config_home, "here_qq", 0x7f007f, 0xddd0dd, TRUE, &style_sets[GEANY_FILETYPES_PERL].styling[22]);
	get_keyfile_hex(config, config_home, "here_qx", 0x7f007f, 0xddd0dd, TRUE, &style_sets[GEANY_FILETYPES_PERL].styling[23]);
	get_keyfile_hex(config, config_home, "string_q", 0x7f007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[24]);
	get_keyfile_hex(config, config_home, "string_qq", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[25]);
	get_keyfile_hex(config, config_home, "string_qx", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[26]);
	get_keyfile_hex(config, config_home, "string_qr", 0x105090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[27]);
	get_keyfile_hex(config, config_home, "string_qw", 0x105090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[28]);
	get_keyfile_hex(config, config_home, "variable_indexer", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[29]);
	get_keyfile_hex(config, config_home, "punctuation", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[30]);
	get_keyfile_hex(config, config_home, "longquote", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[31]);
	get_keyfile_hex(config, config_home, "sub_prototype", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[32]);
	get_keyfile_hex(config, config_home, "format_ident", 0xc000c0, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_PERL].styling[33]);
	get_keyfile_hex(config, config_home, "format", 0xc000c0, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PERL].styling[34]);


	style_sets[GEANY_FILETYPES_PERL].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_PERL, 0, "\
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
	style_sets[GEANY_FILETYPES_PERL].keywords[1] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_PERL].wordchars);
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
	new_style_array(GEANY_FILETYPES_PYTHON, 16);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[0]);
	get_keyfile_hex(config, config_home, "commentline", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[1]);
	get_keyfile_hex(config, config_home, "number", 0x400080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[2]);
	get_keyfile_hex(config, config_home, "string", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[3]);
	get_keyfile_hex(config, config_home, "character", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[4]);
	get_keyfile_hex(config, config_home, "word", 0x600080, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_PYTHON].styling[5]);
	get_keyfile_hex(config, config_home, "triple", 0x008020, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[6]);
	get_keyfile_hex(config, config_home, "tripledouble", 0x404000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[7]);
	get_keyfile_hex(config, config_home, "classname", 0x003030, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[8]);
	get_keyfile_hex(config, config_home, "defname", 0x000080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[9]);
	get_keyfile_hex(config, config_home, "operator", 0x300080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[10]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[11]);
	get_keyfile_hex(config, config_home, "commentblock", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[12]);
	get_keyfile_hex(config, config_home, "stringeol", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[13]);
	get_keyfile_hex(config, config_home, "word2", 0xdd00a6, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_PYTHON].styling[14]);
	get_keyfile_hex(config, config_home, "decorator", 0x808000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PYTHON].styling[15]);

	style_sets[GEANY_FILETYPES_PYTHON].keywords = g_new(gchar*, 3);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_PYTHON, 0, "and as assert break class continue def del elif else except exec finally for from global if import in is lambda not or pass print raise return try while with yield False None True");
	get_keyfile_keywords(config, config_home, "identifiers", GEANY_FILETYPES_PYTHON, 1, "");
	style_sets[GEANY_FILETYPES_PYTHON].keywords[2] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_PYTHON].wordchars);
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
	new_style_array(GEANY_FILETYPES_CMAKE, 15);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CMAKE].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CMAKE].styling[1]);
	get_keyfile_hex(config, config_home, "stringdq", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CMAKE].styling[2]);
	get_keyfile_hex(config, config_home, "stringlq", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CMAKE].styling[3]);
	get_keyfile_hex(config, config_home, "stringrq", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CMAKE].styling[4]);
	get_keyfile_hex(config, config_home, "command", 0x00007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CMAKE].styling[5]);
	get_keyfile_hex(config, config_home, "parameters", 0x991111, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CMAKE].styling[6]);
	get_keyfile_hex(config, config_home, "variable", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CMAKE].styling[7]);
	get_keyfile_hex(config, config_home, "userdefined", 0x0000d0, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CMAKE].styling[8]);
	get_keyfile_hex(config, config_home, "whiledef", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CMAKE].styling[9]);
	get_keyfile_hex(config, config_home, "foreachdef", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CMAKE].styling[10]);
	get_keyfile_hex(config, config_home, "ifdefinedef", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CMAKE].styling[11]);
	get_keyfile_hex(config, config_home, "macrodef", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CMAKE].styling[12]);
	get_keyfile_hex(config, config_home, "stringvar", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CMAKE].styling[13]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CMAKE].styling[14]);

	style_sets[GEANY_FILETYPES_CMAKE].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "commands", GEANY_FILETYPES_CMAKE, 0, "");
	get_keyfile_keywords(config, config_home, "parameters", GEANY_FILETYPES_CMAKE, 1, "");
	get_keyfile_keywords(config, config_home, "userdefined", GEANY_FILETYPES_CMAKE, 2, "");
	style_sets[GEANY_FILETYPES_CMAKE].keywords[3] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_CMAKE].wordchars);
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
	new_style_array(GEANY_FILETYPES_R, 12);

	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_R].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_R].styling[1]);
	get_keyfile_hex(config, config_home, "kword", 0x00007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_R].styling[2]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_R].styling[3]);
	get_keyfile_hex(config, config_home, "basekword", 0x991111, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_R].styling[4]);
	get_keyfile_hex(config, config_home, "otherkword", 0x991111, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_R].styling[5]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_R].styling[6]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_R].styling[7]);
	get_keyfile_hex(config, config_home, "string2", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_R].styling[8]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_R].styling[9]);
	get_keyfile_hex(config, config_home, "infix", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_R].styling[10]);
	get_keyfile_hex(config, config_home, "infixeol", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_R].styling[11]);

	style_sets[GEANY_FILETYPES_R].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_R, 0, "abs array data.frame diag for function if matrix NCOL NROW print read.table require return solve source sqrt sum while");
	get_keyfile_keywords(config, config_home, "package", GEANY_FILETYPES_R, 1, NULL);
	get_keyfile_keywords(config, config_home, "package_other", GEANY_FILETYPES_R, 2, NULL);
	style_sets[GEANY_FILETYPES_R].keywords[3] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_R].wordchars);
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
	new_style_array(GEANY_FILETYPES_RUBY, 35);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[0]);
	get_keyfile_named_style(config, config_home, "commentline", "comment", &style_sets[GEANY_FILETYPES_RUBY].styling[1]);
	get_keyfile_hex(config, config_home, "number", 0x400080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[2]);
	get_keyfile_hex(config, config_home, "string", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[3]);
	get_keyfile_hex(config, config_home, "character", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[4]);
	get_keyfile_hex(config, config_home, "word", 0x111199, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_RUBY].styling[5]);
	get_keyfile_hex(config, config_home, "global", 0x111199, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[6]);
	get_keyfile_hex(config, config_home, "symbol", 0x008020, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[7]);
	get_keyfile_hex(config, config_home, "classname", 0x7f0000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_RUBY].styling[8]);
	get_keyfile_hex(config, config_home, "defname", 0x7f0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[9]);
	get_keyfile_hex(config, config_home, "operator", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[10]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[11]);
	get_keyfile_hex(config, config_home, "modulename", 0x111199, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_RUBY].styling[12]);
	get_keyfile_hex(config, config_home, "backticks", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[13]);
	get_keyfile_hex(config, config_home, "instancevar", 0x000000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_RUBY].styling[14]);
	get_keyfile_hex(config, config_home, "classvar", 0x000000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_RUBY].styling[15]);
	get_keyfile_hex(config, config_home, "datasection", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[16]);
	get_keyfile_hex(config, config_home, "heredelim", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[17]);
	get_keyfile_hex(config, config_home, "worddemoted", 0x111199, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[18]);
	get_keyfile_hex(config, config_home, "stdin", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[19]);
	get_keyfile_hex(config, config_home, "stdout", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[20]);
	get_keyfile_hex(config, config_home, "stderr", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[21]);
	get_keyfile_hex(config, config_home, "datasection", 0x600000, 0xfff0d8, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[22]);
	get_keyfile_hex(config, config_home, "regex", 0x105090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[23]);
	get_keyfile_hex(config, config_home, "here_q", 0x7f007f, 0xddd0dd, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[24]);
	get_keyfile_hex(config, config_home, "here_qq", 0x7f007f, 0xddd0dd, TRUE, &style_sets[GEANY_FILETYPES_RUBY].styling[25]);
	get_keyfile_hex(config, config_home, "here_qx", 0x7f007f, 0xddd0dd, TRUE, &style_sets[GEANY_FILETYPES_RUBY].styling[26]);
	get_keyfile_hex(config, config_home, "string_q", 0x7f007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[27]);
	get_keyfile_hex(config, config_home, "string_qq", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[28]);
	get_keyfile_hex(config, config_home, "string_qx", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[29]);
	get_keyfile_hex(config, config_home, "string_qr", 0x105090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[30]);
	get_keyfile_hex(config, config_home, "string_qw", 0x105090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[31]);
	get_keyfile_hex(config, config_home, "upper_bound", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[32]);
	get_keyfile_hex(config, config_home, "error", 0xe500cc, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[33]);
	get_keyfile_hex(config, config_home, "pod", 0x035650, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_RUBY].styling[34]);

	style_sets[GEANY_FILETYPES_RUBY].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_RUBY, 0, "load define_method attr_accessor attr_writer attr_reader include __FILE__ and def end in or self unless __LINE__ begin defined? ensure module redo super until BEGIN break do false next rescue then when END case else for nil require retry true while alias class elsif if not return undef yield");
	style_sets[GEANY_FILETYPES_RUBY].keywords[1] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_RUBY].wordchars);
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
	new_style_array(GEANY_FILETYPES_SH, 14);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SH].styling[0]);
	get_keyfile_named_style(config, config_home, "commentline", "comment", &style_sets[GEANY_FILETYPES_SH].styling[1]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SH].styling[2]);
	get_keyfile_hex(config, config_home, "word", 0x119911, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_SH].styling[3]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SH].styling[4]);
	get_keyfile_hex(config, config_home, "character", 0x404000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SH].styling[5]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SH].styling[6]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SH].styling[7]);
	get_keyfile_hex(config, config_home, "backticks", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_SH].styling[8]);
	get_keyfile_hex(config, config_home, "param", 0x9f0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SH].styling[9]);
	get_keyfile_hex(config, config_home, "scalar", 0x105090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SH].styling[10]);
	get_keyfile_hex(config, config_home, "error", 0xff0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SH].styling[11]);
	get_keyfile_hex(config, config_home, "here_delim", 0x000000, 0xddd0dd, FALSE, &style_sets[GEANY_FILETYPES_SH].styling[12]);
	get_keyfile_hex(config, config_home, "here_q", 0x7f007f, 0xddd0dd, FALSE, &style_sets[GEANY_FILETYPES_SH].styling[13]);

	style_sets[GEANY_FILETYPES_SH].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_SH, 0, "break case continue do done elif else esac eval exit export fi for goto if in integer return set shift then until while");
	style_sets[GEANY_FILETYPES_SH].keywords[1] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_SH].wordchars);
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
	new_style_array(GEANY_FILETYPES_DOCBOOK, 29);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[0]);
	get_keyfile_hex(config, config_home, "tag", 0x000099, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[1]);
	get_keyfile_hex(config, config_home, "tagunknown", 0xff0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[2]);
	get_keyfile_hex(config, config_home, "attribute", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[3]);
	get_keyfile_hex(config, config_home, "attributeunknown", 0xff0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[4]);
	get_keyfile_hex(config, config_home, "number", 0x800080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[5]);
	get_keyfile_hex(config, config_home, "doublestring", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[6]);
	get_keyfile_hex(config, config_home, "singlestring", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[7]);
	get_keyfile_hex(config, config_home, "other", 0x800080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[8]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[9]);
	get_keyfile_hex(config, config_home, "entity", 0x800080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[10]);
	get_keyfile_hex(config, config_home, "tagend", 0x000099, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[11]);
	get_keyfile_hex(config, config_home, "xmlstart", 0x000099, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[12]);
	get_keyfile_hex(config, config_home, "xmlend", 0x000099, 0xf0f0f0, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[13]);
	get_keyfile_hex(config, config_home, "cdata", 0x660099, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[14]);
	get_keyfile_hex(config, config_home, "question", 0x0000ff, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[15]);
	get_keyfile_hex(config, config_home, "value", 0x660099, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[16]);
	get_keyfile_hex(config, config_home, "xccomment", 0x660099, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[17]);
	get_keyfile_hex(config, config_home, "sgml_default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[18]);
	get_keyfile_hex(config, config_home, "sgml_comment", 0x303030, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[19]);
	get_keyfile_hex(config, config_home, "sgml_special", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[20]);
	get_keyfile_hex(config, config_home, "sgml_command", 0x111199, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[21]);
	get_keyfile_hex(config, config_home, "sgml_doublestring", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[22]);
	get_keyfile_hex(config, config_home, "sgml_simplestring", 0x404000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[23]);
	get_keyfile_hex(config, config_home, "sgml_1st_param", 0x404080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[24]);
	get_keyfile_hex(config, config_home, "sgml_entity", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[25]);
	get_keyfile_hex(config, config_home, "sgml_block_default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[26]);
	get_keyfile_hex(config, config_home, "sgml_1st_param_comment", 0x406090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[27]);
	get_keyfile_hex(config, config_home, "sgml_error", 0xff0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_DOCBOOK].styling[28]);

	style_sets[GEANY_FILETYPES_DOCBOOK].keywords = g_new(gchar*, 3);
	get_keyfile_keywords(config, config_home, "elements", GEANY_FILETYPES_DOCBOOK, 0,
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
	get_keyfile_keywords(config, config_home, "dtd", GEANY_FILETYPES_DOCBOOK, 1, "ELEMENT DOCTYPE ATTLIST ENTITY NOTATION");
	style_sets[GEANY_FILETYPES_DOCBOOK].keywords[2] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_DOCBOOK].wordchars);
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


static void styleset_none(ScintillaObject *sci)
{
	SSM(sci, SCI_SETLEXER, SCLEX_NULL, 0);

	/* we need to set STYLE_DEFAULT before we call SCI_STYLECLEARALL in styleset_common() */
	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_NONE, GCS_DEFAULT);

	styleset_common(sci);

	SSM(sci, SCI_SETWORDCHARS, 0, (sptr_t) common_style_set.wordchars);
	SSM(sci, SCI_SETWHITESPACECHARS, 0, (sptr_t) whitespace_chars);
}


static void styleset_css_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_style_array(GEANY_FILETYPES_CSS, 22);
	get_keyfile_hex(config, config_home, "default", 0x003399, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CSS].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CSS].styling[1]);
	get_keyfile_hex(config, config_home, "tag", 0x2166a4, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CSS].styling[2]);
	get_keyfile_hex(config, config_home, "class", 0x007f00, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CSS].styling[3]);
	get_keyfile_hex(config, config_home, "pseudoclass", 0x660010, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CSS].styling[4]);
	get_keyfile_hex(config, config_home, "unknown_pseudoclass", 0xff0099, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CSS].styling[5]);
	get_keyfile_hex(config, config_home, "unknown_identifier", 0xff0099, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CSS].styling[6]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CSS].styling[7]);
	get_keyfile_hex(config, config_home, "identifier", 0x000099, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CSS].styling[8]);
	get_keyfile_hex(config, config_home, "doublestring", 0x330066, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CSS].styling[9]);
	get_keyfile_hex(config, config_home, "singlestring", 0x330066, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CSS].styling[10]);
	get_keyfile_hex(config, config_home, "attribute", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CSS].styling[11]);
	get_keyfile_hex(config, config_home, "value", 0x303030, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CSS].styling[12]);
	get_keyfile_hex(config, config_home, "id", 0xff9000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CSS].styling[13]);
	get_keyfile_hex(config, config_home, "identifier2", 0x6b6bff, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CSS].styling[14]);
	get_keyfile_hex(config, config_home, "important", 0x990000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CSS].styling[15]);
	get_keyfile_hex(config, config_home, "directive", 0x006bff, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CSS].styling[16]);
	get_keyfile_hex(config, config_home, "identifier3", 0x00c8ff, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CSS].styling[17]);
	get_keyfile_hex(config, config_home, "pseudoelement", 0x666610, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CSS].styling[18]);
	get_keyfile_hex(config, config_home, "extended_identifier", 0x9090a0, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CSS].styling[19]);
	get_keyfile_hex(config, config_home, "extended_pseudoclass", 0x907080, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CSS].styling[20]);
	get_keyfile_hex(config, config_home, "extended_pseudoelement", 0x909080, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CSS].styling[21]);

	style_sets[GEANY_FILETYPES_CSS].keywords = g_new(gchar*, 9);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_CSS, 0, "");
	get_keyfile_keywords(config, config_home, "pseudoclasses", GEANY_FILETYPES_CSS, 1, "");
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_CSS, 2, "");
	get_keyfile_keywords(config, config_home, "css3_properties", GEANY_FILETYPES_CSS, 3, "");
	get_keyfile_keywords(config, config_home, "pseudo_elements", GEANY_FILETYPES_CSS, 4, "");
	get_keyfile_keywords(config, config_home, "browser_css_properties", GEANY_FILETYPES_CSS, 5, "");
	get_keyfile_keywords(config, config_home, "browser_pseudo_classes", GEANY_FILETYPES_CSS, 6, "");
	get_keyfile_keywords(config, config_home, "browser_pseudo_elements", GEANY_FILETYPES_CSS, 7, "");
	style_sets[GEANY_FILETYPES_CSS].keywords[8] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_CSS].wordchars);
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
	new_style_array(GEANY_FILETYPES_NSIS, 19);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_NSIS].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_NSIS].styling[1]);
	get_keyfile_hex(config, config_home, "stringdq", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_NSIS].styling[2]);
	get_keyfile_hex(config, config_home, "stringlq", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_NSIS].styling[3]);
	get_keyfile_hex(config, config_home, "stringrq", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_NSIS].styling[4]);
	get_keyfile_hex(config, config_home, "function", 0x00007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_NSIS].styling[5]);
	get_keyfile_hex(config, config_home, "variable", 0x991111, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_NSIS].styling[6]);
	get_keyfile_hex(config, config_home, "label", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_NSIS].styling[7]);
	get_keyfile_hex(config, config_home, "userdefined", 0x0000d0, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_NSIS].styling[8]);
	get_keyfile_hex(config, config_home, "sectiondef", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_NSIS].styling[9]);
	get_keyfile_hex(config, config_home, "subsectiondef", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_NSIS].styling[10]);
	get_keyfile_hex(config, config_home, "ifdefinedef", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_NSIS].styling[11]);
	get_keyfile_hex(config, config_home, "macrodef", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_NSIS].styling[12]);
	get_keyfile_hex(config, config_home, "stringvar", 0x991111, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_NSIS].styling[13]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_NSIS].styling[14]);
	get_keyfile_hex(config, config_home, "sectiongroup", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_NSIS].styling[15]);
	get_keyfile_hex(config, config_home, "pageex", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_NSIS].styling[16]);
	get_keyfile_hex(config, config_home, "functiondef", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_NSIS].styling[17]);
	get_keyfile_hex(config, config_home, "commentbox", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_NSIS].styling[18]);

	style_sets[GEANY_FILETYPES_NSIS].keywords = g_new(gchar*, 5);
	get_keyfile_keywords(config, config_home, "functions", GEANY_FILETYPES_NSIS, 0, "");
	get_keyfile_keywords(config, config_home, "variables", GEANY_FILETYPES_NSIS, 1, "");
	get_keyfile_keywords(config, config_home, "lables", GEANY_FILETYPES_NSIS, 2, "");
	get_keyfile_keywords(config, config_home, "userdefined", GEANY_FILETYPES_NSIS, 3, "");
	style_sets[GEANY_FILETYPES_NSIS].keywords[4] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_NSIS].wordchars);
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
	new_style_array(GEANY_FILETYPES_PO, 9);
	get_keyfile_hex(config, config_home, "default", 0x7f0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PO].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PO].styling[1]);
	get_keyfile_hex(config, config_home, "msgid", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_PO].styling[2]);
	get_keyfile_hex(config, config_home, "msgid_text", 0x00007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PO].styling[3]);
	get_keyfile_hex(config, config_home, "msgstr", 0x7f0000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_PO].styling[4]);
	get_keyfile_hex(config, config_home, "msgstr_text", 0x7f0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PO].styling[5]);
	get_keyfile_hex(config, config_home, "msgctxt", 0x007f00, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_PO].styling[6]);
	get_keyfile_hex(config, config_home, "msgctxt_text", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_PO].styling[7]);
	get_keyfile_hex(config, config_home, "fuzzy", 0xffa500, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_PO].styling[8]);

	style_sets[GEANY_FILETYPES_PO].keywords = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_PO].wordchars);
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
	new_style_array(GEANY_FILETYPES_CONF, 6);
	get_keyfile_hex(config, config_home, "default", 0x7f0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CONF].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CONF].styling[1]);
	get_keyfile_hex(config, config_home, "section", 0x000090, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CONF].styling[2]);
	get_keyfile_hex(config, config_home, "key", 0x00007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CONF].styling[3]);
	get_keyfile_hex(config, config_home, "assignment", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CONF].styling[4]);
	get_keyfile_hex(config, config_home, "defval", 0x00007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CONF].styling[5]);

	style_sets[GEANY_FILETYPES_CONF].keywords = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_CONF].wordchars);
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
	new_style_array(GEANY_FILETYPES_ASM, 15);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ASM].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ASM].styling[1]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ASM].styling[2]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ASM].styling[3]);
	get_keyfile_hex(config, config_home, "operator", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ASM].styling[4]);
	get_keyfile_hex(config, config_home, "identifier", 0x880000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ASM].styling[5]);
	get_keyfile_hex(config, config_home, "cpuinstruction", 0x111199, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_ASM].styling[6]);
	get_keyfile_hex(config, config_home, "mathinstruction", 0x7f0000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_ASM].styling[7]);
	get_keyfile_hex(config, config_home, "register", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ASM].styling[8]);
	get_keyfile_hex(config, config_home, "directive", 0x3d670f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_ASM].styling[9]);
	get_keyfile_hex(config, config_home, "directiveoperand", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ASM].styling[10]);
	get_keyfile_hex(config, config_home, "commentblock", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ASM].styling[11]);
	get_keyfile_hex(config, config_home, "character", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ASM].styling[12]);
	get_keyfile_hex(config, config_home, "stringeol", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_ASM].styling[13]);
	get_keyfile_hex(config, config_home, "extinstruction", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ASM].styling[14]);

	style_sets[GEANY_FILETYPES_ASM].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "instructions", GEANY_FILETYPES_ASM, 0, "HLT LAD SPI ADD SUB MUL DIV JMP JEZ JGZ JLZ SWAP JSR RET PUSHAC POPAC ADDST SUBST MULST DIVST LSA LDS PUSH POP CLI LDI INK LIA DEK LDX");
	get_keyfile_keywords(config, config_home, "registers", GEANY_FILETYPES_ASM, 1, "");
	get_keyfile_keywords(config, config_home, "directives", GEANY_FILETYPES_ASM, 2, "ORG LIST NOLIST PAGE EQUIVALENT WORD TEXT");
	style_sets[GEANY_FILETYPES_ASM].keywords[3] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_ASM].wordchars);
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
	new_style_array(GEANY_FILETYPES_F77, 15);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_F77].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_F77].styling[1]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_F77].styling[2]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_F77].styling[3]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_F77].styling[4]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_F77].styling[5]);
	get_keyfile_hex(config, config_home, "string2", 0x111199, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_F77].styling[6]);
	get_keyfile_hex(config, config_home, "word", 0x7f0000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_F77].styling[7]);
	get_keyfile_hex(config, config_home, "word2", 0x000099, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_F77].styling[8]);
	get_keyfile_hex(config, config_home, "word3", 0x3d670f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_F77].styling[9]);
	get_keyfile_hex(config, config_home, "preprocessor", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_F77].styling[10]);
	get_keyfile_hex(config, config_home, "operator2", 0x301010, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_F77].styling[11]);
	get_keyfile_hex(config, config_home, "continuation", 0x000000, 0xf0e080, FALSE, &style_sets[GEANY_FILETYPES_F77].styling[12]);
	get_keyfile_hex(config, config_home, "stringeol", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_F77].styling[13]);
	get_keyfile_hex(config, config_home, "label", 0xa861a8, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_F77].styling[14]);

	style_sets[GEANY_FILETYPES_F77].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_F77, 0, "");
	get_keyfile_keywords(config, config_home, "intrinsic_functions", GEANY_FILETYPES_F77, 1, "");
	get_keyfile_keywords(config, config_home, "user_functions", GEANY_FILETYPES_F77, 2, "");
	style_sets[GEANY_FILETYPES_F77].keywords[3] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_F77].wordchars);
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
	new_style_array(GEANY_FILETYPES_FORTRAN, 15);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[1]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[2]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[3]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[4]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[5]);
	get_keyfile_hex(config, config_home, "string2", 0x111199, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[6]);
	get_keyfile_hex(config, config_home, "word", 0x7f0000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[7]);
	get_keyfile_hex(config, config_home, "word2", 0x000099, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[8]);
	get_keyfile_hex(config, config_home, "word3", 0x3d670f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[9]);
	get_keyfile_hex(config, config_home, "preprocessor", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[10]);
	get_keyfile_hex(config, config_home, "operator2", 0x301010, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[11]);
	get_keyfile_hex(config, config_home, "continuation", 0x000000, 0xf0e080, FALSE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[12]);
	get_keyfile_hex(config, config_home, "stringeol", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[13]);
	get_keyfile_hex(config, config_home, "label", 0xa861a8, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_FORTRAN].styling[14]);

	style_sets[GEANY_FILETYPES_FORTRAN].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_FORTRAN, 0, "");
	get_keyfile_keywords(config, config_home, "intrinsic_functions", GEANY_FILETYPES_FORTRAN, 1, "");
	get_keyfile_keywords(config, config_home, "user_functions", GEANY_FILETYPES_FORTRAN, 2, "");
	style_sets[GEANY_FILETYPES_FORTRAN].keywords[3] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_FORTRAN].wordchars);
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
	new_style_array(GEANY_FILETYPES_SQL, 15);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SQL].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SQL].styling[1]);
	get_keyfile_hex(config, config_home, "commentline", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SQL].styling[2]);
	get_keyfile_hex(config, config_home, "commentdoc", 0x3f5fbf, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SQL].styling[3]);
	get_keyfile_hex(config, config_home, "number", 0x7f7f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SQL].styling[4]);
	get_keyfile_hex(config, config_home, "word", 0x001a7f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_SQL].styling[5]);
	get_keyfile_hex(config, config_home, "word2", 0x7f0000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_SQL].styling[6]);
	get_keyfile_hex(config, config_home, "string", 0x7f007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SQL].styling[7]);
	get_keyfile_hex(config, config_home, "character", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SQL].styling[8]);
	get_keyfile_hex(config, config_home, "operator", 0x000000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_SQL].styling[9]);
	get_keyfile_hex(config, config_home, "identifier", 0x111199, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SQL].styling[10]);
	get_keyfile_hex(config, config_home, "sqlplus", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SQL].styling[11]);
	get_keyfile_hex(config, config_home, "sqlplus_prompt", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SQL].styling[12]);
	get_keyfile_hex(config, config_home, "sqlplus_comment", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SQL].styling[13]);
	get_keyfile_hex(config, config_home, "quotedidentifier", 0x111199, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_SQL].styling[14]);

	style_sets[GEANY_FILETYPES_SQL].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_SQL, 0,
						"absolute action add admin after aggregate \
						alias all allocate alter and any are array as asc \
						assertion at authorization before begin bfile bigint binary bit blob bool boolean both breadth by \
						call cascade cascaded case cast catalog char character check class clob close cluster collate \
						collation column commit completion connect connection constraint constraints \
						constructor continue corresponding create cross cube current \
						current_date current_path current_role current_time current_timestamp \
						current_user cursor cycle data date day deallocate dec decimal declare default \
						deferrable deferred delete depth deref desc describe descriptor destroy destructor \
						deterministic diagnostics dictionary dimension disconnect diskgroup distinct domain double drop dynamic \
						each else end end-exec equals escape every except exception exec execute exists explain external \
						false fetch first fixed flashback float for foreign found from free full function general get global \
						go goto grant group grouping having host hour identity if ignore immediate in index indextype indicator \
						initialize initially inner inout input insert int integer intersect interval \
						into is isolation iterate join key language large last lateral leading left less level like \
						limit local localtime localtimestamp locator long map match materialized mediumblob mediumint mediumtext \
						merge middleint minus minute modifies modify module month names national natural nchar nclob new next no noaudit \
						none not null numeric nvarchar2 object of off old on only open operation option or order ordinality out outer \
						output package pad parameter parameters partial path postfix precision prefix preorder prepare preserve primary \
						prior privileges procedure profile public purge read reads real recursive ref references referencing regexp regexp_like \
						relative rename restrict result return returning returns revoke right role rollback rollup routine row rows \
						savepoint schema scroll scope search second section select sequence session session_user set sets size \
						smallint some space specific specifictype sql sqlexception sqlstate sqlwarning start state statement static \
						structure synonym system_user table tablespace temporary terminate than then time timestamp \
						timezone_hour timezone_minute tinyint to trailing transaction translation  treat trigger true truncate \
						type under union unique unknown unnest update usage user using value values varchar varchar2 variable varying \
						view when whenever where with without work write year zone");
	style_sets[GEANY_FILETYPES_SQL].keywords[1] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_SQL].wordchars);
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


static void styleset_haskell_init(gint ft_id, GKeyFile *config, GKeyFile *config_home)
{
	new_style_array(GEANY_FILETYPES_HASKELL, 17);

	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[0]);
	get_keyfile_hex(config, config_home, "commentline", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[1]);
	get_keyfile_hex(config, config_home, "commentblock", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[2]);
	get_keyfile_hex(config, config_home, "commentblock2", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[3]);
	get_keyfile_hex(config, config_home, "commentblock3", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[4]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[5]);
	get_keyfile_hex(config, config_home, "keyword", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_HASKELL].styling[6]);
	get_keyfile_hex(config, config_home, "import", 0x991111, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[7]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[8]);
	get_keyfile_hex(config, config_home, "character", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[9]);
	get_keyfile_hex(config, config_home, "class", 0x0000d0, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[10]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[11]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[12]);
	get_keyfile_hex(config, config_home, "instance", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[13]);
	get_keyfile_hex(config, config_home, "capital", 0x635b00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[14]);
	get_keyfile_hex(config, config_home, "module", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[15]);
	get_keyfile_hex(config, config_home, "data", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_HASKELL].styling[16]);

	style_sets[GEANY_FILETYPES_HASKELL].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_HASKELL, 0,
			"as case class data deriving do else if import in infixl infixr instance let module of primitive qualified then type where");
	style_sets[GEANY_FILETYPES_HASKELL].keywords[1] = NULL;

	get_keyfile_wordchars(config, config_home, &style_sets[GEANY_FILETYPES_HASKELL].wordchars);
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
	new_style_array(GEANY_FILETYPES_CAML, 14);

	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CAML].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CAML].styling[1]);
	get_keyfile_hex(config, config_home, "comment1", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CAML].styling[2]);
	get_keyfile_hex(config, config_home, "comment2", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CAML].styling[3]);
	get_keyfile_hex(config, config_home, "comment3", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CAML].styling[4]);
	get_keyfile_hex(config, config_home, "number", 0x7f7f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CAML].styling[5]);
	get_keyfile_hex(config, config_home, "keyword", 0x001a7f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CAML].styling[6]);
	get_keyfile_hex(config, config_home, "keyword2", 0x7f0000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_CAML].styling[7]);
	get_keyfile_hex(config, config_home, "string", 0x7f007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CAML].styling[8]);
	get_keyfile_hex(config, config_home, "char", 0x7f007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CAML].styling[9]);
	get_keyfile_hex(config, config_home, "operator", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CAML].styling[10]);
	get_keyfile_hex(config, config_home, "identifier", 0x111199, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_CAML].styling[11]);
	get_keyfile_hex(config, config_home, "tagname", 0x000000, 0xffe0ff, TRUE, &style_sets[GEANY_FILETYPES_CAML].styling[12]);
	get_keyfile_hex(config, config_home, "linenum", 0x000000, 0xc0c0c0, FALSE, &style_sets[GEANY_FILETYPES_CAML].styling[13]);

	style_sets[GEANY_FILETYPES_CAML].keywords = g_new(gchar*, 3);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_CAML, 0,
			"and as assert asr begin class constraint do \
			done downto else end exception external false for fun function functor if in include inherit \
			initializer land lazy let lor lsl lsr lxor match method mod module mutable new object of open \
			or private rec sig struct then to true try type val virtual when while with");
	get_keyfile_keywords(config, config_home, "keywords_optional", GEANY_FILETYPES_CAML, 1, "option Some None ignore ref");
	style_sets[GEANY_FILETYPES_CAML].keywords[2] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_CAML].wordchars);
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
	new_style_array(GEANY_FILETYPES_TCL, 16);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_TCL].styling[0]);
	get_keyfile_named_style(config, config_home, "comment", "comment", &style_sets[GEANY_FILETYPES_TCL].styling[1]);
	get_keyfile_named_style(config, config_home, "commentline", "comment", &style_sets[GEANY_FILETYPES_TCL].styling[2]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_TCL].styling[3]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_TCL].styling[4]);
	get_keyfile_hex(config, config_home, "identifier", 0xa20000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_TCL].styling[5]);
	get_keyfile_hex(config, config_home, "wordinquote", 0x7f007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_TCL].styling[6]);
	get_keyfile_hex(config, config_home, "inquote", 0x7f007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_TCL].styling[7]);
	get_keyfile_hex(config, config_home, "substitution", 0x111199, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_TCL].styling[8]);
	get_keyfile_hex(config, config_home, "modifier", 0x7f007f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_TCL].styling[9]);
	get_keyfile_hex(config, config_home, "expand", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_TCL].styling[10]);
	get_keyfile_hex(config, config_home, "wordtcl", 0x111199, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_TCL].styling[11]);
	get_keyfile_hex(config, config_home, "wordtk", 0x7f0000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_TCL].styling[12]);
	get_keyfile_hex(config, config_home, "worditcl", 0x111199, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_TCL].styling[13]);
	get_keyfile_hex(config, config_home, "wordtkcmds", 0x7f0000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_TCL].styling[14]);
	get_keyfile_hex(config, config_home, "wordexpand", 0x7f0000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_TCL].styling[15]);

	style_sets[GEANY_FILETYPES_TCL].keywords = g_new(gchar*, 6);
	get_keyfile_keywords(config, config_home, "tcl", GEANY_FILETYPES_TCL, 0, "");
	get_keyfile_keywords(config, config_home, "tk", GEANY_FILETYPES_TCL, 1, "");
	get_keyfile_keywords(config, config_home, "itcl", GEANY_FILETYPES_TCL, 2, "");
	get_keyfile_keywords(config, config_home, "tkcommands", GEANY_FILETYPES_TCL, 3, "");
	get_keyfile_keywords(config, config_home, "expand", GEANY_FILETYPES_TCL, 4, "");
	style_sets[GEANY_FILETYPES_TCL].keywords[5] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_TCL].wordchars);
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
	new_style_array(GEANY_FILETYPES_MATLAB, 9);
	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_MATLAB].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_MATLAB].styling[1]);
	get_keyfile_hex(config, config_home, "command", 0x111199, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_MATLAB].styling[2]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_MATLAB].styling[3]);
	get_keyfile_hex(config, config_home, "keyword", 0x001a7f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_MATLAB].styling[4]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_MATLAB].styling[5]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_MATLAB].styling[6]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_MATLAB].styling[7]);
	get_keyfile_hex(config, config_home, "doublequotedstring", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_MATLAB].styling[8]);

	style_sets[GEANY_FILETYPES_MATLAB].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_MATLAB, 0, "break case catch continue else elseif end for function global if otherwise persistent return switch try while");
	style_sets[GEANY_FILETYPES_MATLAB].keywords[1] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_MATLAB].wordchars);
}


static void styleset_matlab(ScintillaObject *sci)
{
	const filetype_id ft_id = GEANY_FILETYPES_MATLAB;

	styleset_common(sci);

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
	new_style_array(GEANY_FILETYPES_D, 18);

	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_D].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0xd00000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_D].styling[1]);
	get_keyfile_hex(config, config_home, "commentline", 0xd00000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_D].styling[2]);
	get_keyfile_hex(config, config_home, "commentdoc", 0x3f5fbf, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_D].styling[3]);
	get_keyfile_hex(config, config_home, "commentdocnested", 0x3f5fbf, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_D].styling[4]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_D].styling[5]);
	get_keyfile_hex(config, config_home, "word", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_D].styling[6]);
	get_keyfile_hex(config, config_home, "word2", 0x991111, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_D].styling[7]);
	get_keyfile_hex(config, config_home, "word3", 0x991111, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_D].styling[8]);
	get_keyfile_hex(config, config_home, "typedef", 0x0000d0, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_D].styling[9]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_D].styling[10]);
	get_keyfile_hex(config, config_home, "stringeol", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_D].styling[11]);
	get_keyfile_hex(config, config_home, "character", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_D].styling[12]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_D].styling[13]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_D].styling[14]);
	get_keyfile_hex(config, config_home, "commentlinedoc", 0x3f5fbf, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_D].styling[15]);
	get_keyfile_hex(config, config_home, "commentdockeyword", 0x3f5fbf, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_D].styling[16]);
	get_keyfile_hex(config, config_home, "commentdockeyworderror", 0x3f5fbf, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_D].styling[17]);

	style_sets[GEANY_FILETYPES_D].keywords = g_new(gchar*, 5);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_D, 0,
			"__FILE__ __LINE__ __DATA__ __TIME__ __TIMESTAMP__ abstract alias align asm assert auto \
			 body bool break byte case cast catch cdouble cent cfloat char class const continue creal \
			 dchar debug default delegate delete deprecated do double else enum export extern false \
			 final finally float for foreach function goto idouble if ifloat import in inout int \
			 interface invariant ireal is long mixin module new null out override package pragma \
			 private protected public real return scope short static struct super switch \
			 synchronized template this throw true try typedef typeof ubyte ucent uint ulong union \
			 unittest ushort version void volatile wchar while with");
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_D, 1,
			"");
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_D, 2,
			"Authors Bugs Copyright Date Deprecated Examples History License Macros Params Returns \
			 See_Also Standards Throws Version");
	get_keyfile_keywords(config, config_home, "types", GEANY_FILETYPES_D, 3,
			"");
	style_sets[GEANY_FILETYPES_D].keywords[4] = NULL;

	get_keyfile_wordchars(config, config_home, &style_sets[GEANY_FILETYPES_D].wordchars);
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
	new_style_array(GEANY_FILETYPES_FERITE, 20);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_FERITE);

	style_sets[GEANY_FILETYPES_FERITE].keywords = g_new(gchar*, 4);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_FERITE, 0, "false null self super true abstract alias and arguments attribute_missing break case class closure conformsToProtocol constructor continue default deliver destructor diliver directive do else extends eval final fix for function global handle if iferr implements include instanceof isa method_missing modifies monitor namespace new or private protected protocol public raise recipient rename return static switch uses using while");
	get_keyfile_keywords(config, config_home, "types", GEANY_FILETYPES_FERITE, 1, "boolean string number array object void");
	get_keyfile_keywords(config, config_home, "docComment", GEANY_FILETYPES_FERITE, 2, "brief class declaration description end example extends function group implements modifies module namespace param protocol return return static type variable warning");
	style_sets[GEANY_FILETYPES_FERITE].keywords[3] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_FERITE].wordchars);
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
	new_style_array(GEANY_FILETYPES_VHDL, 15);

	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_VHDL].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0xd00000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_VHDL].styling[1]);
	get_keyfile_hex(config, config_home, "comment_line_bang", 0x3f5fbf, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_VHDL].styling[2]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_VHDL].styling[3]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_VHDL].styling[4]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_VHDL].styling[5]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_VHDL].styling[6]);
	get_keyfile_hex(config, config_home, "stringeol", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_VHDL].styling[7]);
	get_keyfile_hex(config, config_home, "keyword", 0x001a7f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_VHDL].styling[8]);
	get_keyfile_hex(config, config_home, "stdoperator", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_VHDL].styling[9]);
	get_keyfile_hex(config, config_home, "attribute", 0x804020, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_VHDL].styling[10]);
	get_keyfile_hex(config, config_home, "stdfunction", 0x808020, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_VHDL].styling[11]);
	get_keyfile_hex(config, config_home, "stdpackage", 0x208020, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_VHDL].styling[12]);
	get_keyfile_hex(config, config_home, "stdtype", 0x208080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_VHDL].styling[13]);
	get_keyfile_hex(config, config_home, "userword", 0x804020, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_VHDL].styling[14]);

	style_sets[GEANY_FILETYPES_VHDL].keywords = g_new(gchar*, 8);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_VHDL, 0,
			"access after alias all architecture array assert attribute begin block \
			 body buffer bus case component configuration constant disconnect downto else elsif \
			 end entity exit file for function generate generic group guarded if impure in inertial \
			 inout is label library linkage literal loop map new next null of on open others out \
			 package port postponed procedure process pure range record register reject report \
			 return select severity shared signal subtype then to transport type unaffected units \
			 until use variable wait when while with");
	get_keyfile_keywords(config, config_home, "operators", GEANY_FILETYPES_VHDL, 1,
			"abs and mod nand nor not or rem rol ror sla sll sra srl xnor xor");
	get_keyfile_keywords(config, config_home, "attributes", GEANY_FILETYPES_VHDL, 2,
			"left right low high ascending image value pos val succ pred leftof rightof base range \
			 reverse_range length delayed stable quiet transaction event active last_event last_active \
			 last_value driving driving_value simple_name path_name instance_name");
	get_keyfile_keywords(config, config_home, "std_functions", GEANY_FILETYPES_VHDL, 3,
			"now readline read writeline write endfile resolved to_bit to_bitvector to_stdulogic \
			 to_stdlogicvector to_stdulogicvector to_x01 to_x01z to_UX01 rising_edge falling_edge \
			 is_x shift_left shift_right rotate_left rotate_right resize to_integer to_unsigned \
			 to_signed std_match to_01");
	get_keyfile_keywords(config, config_home, "std_packages", GEANY_FILETYPES_VHDL, 4,
			"std ieee work standard textio std_logic_1164 std_logic_arith std_logic_misc \
			 std_logic_signed std_logic_textio std_logic_unsigned numeric_bit numeric_std \
			 math_complex math_real vital_primitives vital_timing");
	get_keyfile_keywords(config, config_home, "std_types", GEANY_FILETYPES_VHDL, 5,
			"boolean bit character severity_level integer real time delay_length natural positive \
			 string bit_vector file_open_kind file_open_status line text side width std_ulogic \
			 std_ulogic_vector std_logic std_logic_vector X01 X01Z UX01 UX01Z unsigned signed");
	get_keyfile_keywords(config, config_home, "userwords", GEANY_FILETYPES_VHDL, 6, "");
	style_sets[GEANY_FILETYPES_VHDL].keywords[7] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_VHDL].wordchars);
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
	new_style_array(GEANY_FILETYPES_YAML, 10);

	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_YAML].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_YAML].styling[1]);
	get_keyfile_hex(config, config_home, "identifier", 0x000088, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_YAML].styling[2]);
	get_keyfile_hex(config, config_home, "keyword", 0x991111, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_YAML].styling[3]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_YAML].styling[4]);
	get_keyfile_hex(config, config_home, "reference", 0x008888, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_YAML].styling[5]);
	get_keyfile_hex(config, config_home, "document", 0x000088, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_YAML].styling[6]);
	get_keyfile_hex(config, config_home, "text", 0x333366, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_YAML].styling[7]);
	get_keyfile_hex(config, config_home, "error", 0xff0000, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_YAML].styling[8]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_YAML].styling[9]);

	style_sets[GEANY_FILETYPES_YAML].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_YAML, 0, "true false yes no");
	style_sets[GEANY_FILETYPES_YAML].keywords[1] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_VHDL].wordchars);
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
	new_style_array(GEANY_FILETYPES_JS, 20);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_JS);

	style_sets[GEANY_FILETYPES_JS].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_JS, 0, "\
			abs abstract acos anchor asin atan atan2 big bold boolean break byte \
			case catch ceil char charAt charCodeAt class concat const continue cos \
			Date debugger default delete do double else enum escape eval exp export \
			extends false final finally fixed float floor fontcolor fontsize for \
			fromCharCode function goto if implements import in indexOf Infinity \
			instanceof int interface isFinite isNaN italics join lastIndexOf length \
			link log long Math max MAX_VALUE min MIN_VALUE NaN native NEGATIVE_INFINITY \
			new null Number package parseFloat parseInt pop POSITIVE_INFINITY pow private \
			protected public push random return reverse round shift short sin slice small \
			sort splice split sqrt static strike string String sub substr substring sup \
			super switch synchronized tan this throw throws toLowerCase toString \
			toUpperCase transient true try typeof undefined unescape unshift valueOf \
			var void volatile while with");
	style_sets[GEANY_FILETYPES_JS].keywords[1] = NULL;

	get_keyfile_wordchars(config, config_home, &style_sets[GEANY_FILETYPES_JS].wordchars);
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
	new_style_array(GEANY_FILETYPES_LUA, 20);

	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0xd00000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[1]);
	get_keyfile_hex(config, config_home, "commentline", 0xd00000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[2]);
	get_keyfile_hex(config, config_home, "commentdoc", 0x3f5fbf, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_LUA].styling[3]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[4]);
	get_keyfile_hex(config, config_home, "word", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_LUA].styling[5]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[6]);
	get_keyfile_hex(config, config_home, "character", 0x008000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[7]);
	get_keyfile_hex(config, config_home, "literalstring", 0x008020, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[8]);
	get_keyfile_hex(config, config_home, "preprocessor", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[9]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[10]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[11]);
	get_keyfile_hex(config, config_home, "stringeol", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[12]);
	get_keyfile_hex(config, config_home, "function_basic", 0x991111, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[13]);
	get_keyfile_hex(config, config_home, "function_other", 0x690000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[14]);
	get_keyfile_hex(config, config_home, "coroutines", 0x66005c, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[15]);
	get_keyfile_hex(config, config_home, "word5", 0x7979ff, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[16]);
	get_keyfile_hex(config, config_home, "word6", 0xad00ff, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[17]);
	get_keyfile_hex(config, config_home, "word7", 0x03D000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[18]);
	get_keyfile_hex(config, config_home, "word8", 0xff7600, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_LUA].styling[19]);

	style_sets[GEANY_FILETYPES_LUA].keywords = g_new(gchar*, 9);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_LUA, 0,
			"and break do else elseif end false for function if \
			 in local nil not or repeat return then true until while");
	get_keyfile_keywords(config, config_home, "function_basic", GEANY_FILETYPES_LUA, 1,
			"_VERSION assert collectgarbage dofile error gcinfo loadfile loadstring \
			 print rawget rawset require tonumber tostring type unpack \
			 _ALERT _ERRORMESSAGE _INPUT _PROMPT _OUTPUT \
			 _STDERR _STDIN _STDOUT call dostring foreach foreachi getn globals newtype \
			 sort tinsert tremove _G getfenv getmetatable ipairs loadlib next pairs pcall \
			 rawequal setfenv setmetatable xpcall string table math coroutine io os debug \
			 load module select");
	get_keyfile_keywords(config, config_home, "function_other", GEANY_FILETYPES_LUA, 2,
			"abs acos asin atan atan2 ceil cos deg exp \
			 floor format frexp gsub ldexp log log10 max min mod rad random randomseed \
			 sin sqrt strbyte strchar strfind strlen strlower strrep strsub strupper tan \
			 string.byte string.char string.dump string.find string.len \
			 string.lower string.rep string.sub string.upper string.format string.gfind string.gsub \
			 table.concat table.foreach table.foreachi table.getn table.sort table.insert table.remove table.setn \
			 math.abs math.acos math.asin math.atan math.atan2 math.ceil math.cos math.deg math.exp \
			 math.floor math.frexp math.ldexp math.log math.log10 math.max math.min math.mod \
			 math.pi math.pow math.rad math.random math.randomseed math.sin math.sqrt math.tan \
			 string.gmatch string.match string.reverse table.maxn \
			 math.cosh math.fmod math.modf math.sinh math.tanh math.huge");
	get_keyfile_keywords(config, config_home, "coroutines", GEANY_FILETYPES_LUA, 3,
			"openfile closefile readfrom writeto appendto remove rename flush seek tmpfile tmpname \
			 read write clock date difftime execute exit getenv setlocale time coroutine.create \
			 coroutine.resume coroutine.status coroutine.wrap coroutine.yield io.close io.flush \
			 io.input io.lines io.open io.output io.read io.tmpfile io.type io.write io.stdin \
			 io.stdout io.stderr os.clock os.date os.difftime os.execute os.exit os.getenv \
			 os.remove os.rename os.setlocale os.time os.tmpname coroutine.running package.cpath \
			 package.loaded package.loadlib package.path package.preload package.seeall io.popen");
	get_keyfile_keywords(config, config_home, "user1", GEANY_FILETYPES_LUA, 4, "");
	get_keyfile_keywords(config, config_home, "user2", GEANY_FILETYPES_LUA, 5, "");
	get_keyfile_keywords(config, config_home, "user3", GEANY_FILETYPES_LUA, 6, "");
	get_keyfile_keywords(config, config_home, "user4", GEANY_FILETYPES_LUA, 7, "");
	style_sets[GEANY_FILETYPES_LUA].keywords[8] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_LUA].wordchars);
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
	new_style_array(GEANY_FILETYPES_BASIC, 19);

	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[0]);
	get_keyfile_hex(config, config_home, "comment", 0x808080, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[1]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[2]);
	get_keyfile_hex(config, config_home, "word", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_BASIC].styling[3]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[4]);
	get_keyfile_hex(config, config_home, "preprocessor", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[5]);
	get_keyfile_hex(config, config_home, "operator", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[6]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[7]);
	get_keyfile_hex(config, config_home, "date", 0x1a6500, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[8]);
	get_keyfile_hex(config, config_home, "stringeol", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[9]);
	get_keyfile_hex(config, config_home, "word2", 0x007f7f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_BASIC].styling[10]);
	get_keyfile_hex(config, config_home, "word3", 0x991111, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[11]);
	get_keyfile_hex(config, config_home, "word4", 0x0000d0, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[12]);
	get_keyfile_hex(config, config_home, "constant", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[13]);
	get_keyfile_hex(config, config_home, "asm", 0x105090, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[14]);
	get_keyfile_hex(config, config_home, "label", 0x007f7f, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[15]);
	get_keyfile_hex(config, config_home, "error", 0xd00000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[16]);
	get_keyfile_hex(config, config_home, "hexnumber", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[17]);
	get_keyfile_hex(config, config_home, "binnumber", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_BASIC].styling[18]);

	style_sets[GEANY_FILETYPES_BASIC].keywords = g_new(gchar*, 5);
	get_keyfile_keywords(config, config_home, "keywords", GEANY_FILETYPES_BASIC, 0,
			"as asm bit bitreset bitset byte case cint close cls color const \
			 continue cshort csign csng cubyte cuint culngint custom data \
			 dim do double  else elseif end enum environ eof err error exec exit exp \
			 export extern field fix for function get gosub goto hex hibyte hiword if iif imp \
			 input instr int integer is kill left len let lobyte loc local locate lof log long \
			 longint loop loword lset mklongint mks mkshort mod next not on once open or out \
			 pointer pos preserve preset private public put read redim rem reset restore return \
			 sizeof sleep space static step stop str string sub then time timer to type ubound \
			 ubyte ucase uinteger ulongint union unsigned until ushort using val val64 valint \
			 wait while with xor");
	get_keyfile_keywords(config, config_home, "preprocessor", GEANY_FILETYPES_BASIC, 1,
			"#define defined #dynamic #else #endif #endmacro #error #if #ifdef #ifndef #inclib #include \
			 #libpath #line #macro #print #undef");
	get_keyfile_keywords(config, config_home, "user1", GEANY_FILETYPES_BASIC, 2, "");
	get_keyfile_keywords(config, config_home, "user2", GEANY_FILETYPES_BASIC, 3, "");
	style_sets[GEANY_FILETYPES_BASIC].keywords[4] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_BASIC].wordchars);
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
	new_style_array(GEANY_FILETYPES_AS, 20);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_AS);

	style_sets[GEANY_FILETYPES_AS].keywords = g_new(gchar *, 4);

	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_AS, 0, "");
	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_AS, 1, "");
	get_keyfile_keywords(config, config_home, "classes", GEANY_FILETYPES_AS, 2, "");
	style_sets[GEANY_FILETYPES_AS].keywords[3] = NULL;

	get_keyfile_wordchars(config, config_home, &style_sets[GEANY_FILETYPES_AS].wordchars);
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
	new_style_array(GEANY_FILETYPES_HAXE, 20);
	styleset_c_like_init(config, config_home, GEANY_FILETYPES_HAXE);

	style_sets[GEANY_FILETYPES_HAXE].keywords = g_new(gchar*, 4);

	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_HAXE, 0, "\
			abstract break case catch class \
			continue default do else enum external extends \
			finally float for function goto if implements import in \
			interface new package protected public \
			return static super switch this throw throws \
			try type var while");

	get_keyfile_keywords(config, config_home, "secondary", GEANY_FILETYPES_HAXE, 1, "\
			Bool Enum Float Int Null Void Dynamic String");

	get_keyfile_keywords(config, config_home, "classes", GEANY_FILETYPES_HAXE, 2, "\
			Array ArrayAccess Class Date DateTools \
			EReg Enum Hash IntHash IntIter \
			Iterable Iterator Lambda List Math Protected \
			Reflect Std  StringBuf StringTools Type \
			UInt ValueType Void Xml XmlType");

	style_sets[GEANY_FILETYPES_HAXE].keywords[3] = NULL;

	get_keyfile_wordchars(config, config_home, &style_sets[GEANY_FILETYPES_HAXE].wordchars);
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
	new_style_array(GEANY_FILETYPES_ADA, 12);

	get_keyfile_hex(config, config_home, "default", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ADA].styling[0]);
	get_keyfile_hex(config, config_home, "word", 0x00007f, 0xffffff, TRUE, &style_sets[GEANY_FILETYPES_ADA].styling[1]);
	get_keyfile_hex(config, config_home, "identifier", 0x000000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ADA].styling[2]);
	get_keyfile_hex(config, config_home, "number", 0x007f00, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ADA].styling[3]);
	get_keyfile_hex(config, config_home, "delimiter", 0x301010, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ADA].styling[4]);
	get_keyfile_hex(config, config_home, "character", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ADA].styling[5]);
	get_keyfile_hex(config, config_home, "charactereol", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_ADA].styling[6]);
	get_keyfile_hex(config, config_home, "string", 0xff901e, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ADA].styling[7]);
	get_keyfile_hex(config, config_home, "stringeol", 0x000000, 0xe0c0e0, FALSE, &style_sets[GEANY_FILETYPES_ADA].styling[8]);
	get_keyfile_hex(config, config_home, "label", 0xaaaaaa, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ADA].styling[9]);
	get_keyfile_hex(config, config_home, "commentline", 0xd00000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ADA].styling[10]);
	get_keyfile_hex(config, config_home, "illegal", 0xff0000, 0xffffff, FALSE, &style_sets[GEANY_FILETYPES_ADA].styling[11]);

	style_sets[GEANY_FILETYPES_ADA].keywords = g_new(gchar*, 2);
	get_keyfile_keywords(config, config_home, "primary", GEANY_FILETYPES_ADA, 0, "abort abs abstract accept access aliased all and array at begin body case constant declare delay delta digits do else elsif end entry exception exit for function generic goto if in interface is limited loop mod new not null of or others out overriding package pragma private procedure protected raise range record rem renames requeue return reverse select separate subtype synchronized tagged task terminate then type until use when while with xor");
	style_sets[GEANY_FILETYPES_ADA].keywords[1] = NULL;

	get_keyfile_wordchars(config, config_home,
		&style_sets[GEANY_FILETYPES_ADA].wordchars);
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
	/* Clear old information if necessary - e.g. reloading config */
	styleset_free(filetype_idx);

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
		default:
		styleset_case(GEANY_FILETYPES_NONE,		styleset_none);
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
	if (ft_id < 0 || ft_id > GEANY_MAX_BUILT_IN_FILETYPES)
		return NULL;

	/* ensure filetype loaded */
	filetypes_load_config(ft_id, FALSE);

	/* TODO: style_id might not be the real array index (Scintilla styles are not always synced
	 * with array indices) */
	return get_style(ft_id, style_id);
}
