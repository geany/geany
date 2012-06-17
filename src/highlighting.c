/*
 *      highlighting.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2011-2012 Colomban Wendling <ban(at)herbesfolles(dot)org>
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
#include "main.h"
#include "support.h"
#include "sciwrappers.h"
#include "document.h"
#include "dialogs.h"
#include "filetypesprivate.h"

#include "highlightingmappings.h"


#define GEANY_COLORSCHEMES_SUBDIR "colorschemes"

/* Whitespace has to be set after setting wordchars. */
#define GEANY_WHITESPACE_CHARS " \t" "!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~"


static gchar *whitespace_chars;


typedef struct
{
	gsize			count;		/* number of styles */
	GeanyLexerStyle	*styling;		/* array of styles, NULL if not used or uninitialised */
	gchar			**keywords;
	gchar			*wordchars;	/* NULL used for style sets with no styles */
	gchar			**property_keys;
	gchar			**property_values;
} StyleSet;

/* each filetype has a styleset but GEANY_FILETYPES_NONE uses common_style_set for styling */
static StyleSet *style_sets = NULL;


enum	/* Geany common styling */
{
	GCS_DEFAULT,
	GCS_SELECTION,
	GCS_BRACE_GOOD,
	GCS_BRACE_BAD,
	GCS_MARGIN_LINENUMBER,
	GCS_MARGIN_FOLDING,
	GCS_FOLD_SYMBOL_HIGHLIGHT,
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
	GCS_CALLTIPS,
	GCS_MAX
};

static struct
{
	GeanyLexerStyle	 styling[GCS_MAX];

	/* icon style, 1-4 */
	gint fold_marker;
	/* vertical line style, 0-2 */
	gint fold_lines;
	/* horizontal line when folded, 0-2 */
	gint fold_draw_line;

	gchar			*wordchars;
} common_style_set;


/* For filetypes.common [named_styles] section.
 * 0xBBGGRR format.
 * e.g. "comment" => &GeanyLexerStyle{0x0000d0, 0xffffff, FALSE, FALSE} */
static GHashTable *named_style_hash = NULL;

/* 0xBBGGRR format, set by "default" named style. */
static GeanyLexerStyle gsd_default = {0x000000, 0xffffff, FALSE, FALSE};


/* Note: use sciwrappers.h instead where possible.
 * Do not use SSM in files unrelated to scintilla. */
#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)

/* filetypes should use the filetypes.foo [lexer_properties] group instead of hardcoding */
static void sci_set_property(ScintillaObject *sci, const gchar *name, const gchar *value)
{
	SSM(sci, SCI_SETPROPERTY, (uptr_t) name, (sptr_t) value);
}


static void new_styleset(guint file_type_id, gsize styling_count)
{
	StyleSet *set = &style_sets[file_type_id];

	set->count = styling_count;
	set->styling = g_new0(GeanyLexerStyle, styling_count);
}


static void free_styleset(guint file_type_id)
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
	g_strfreev(style_ptr->property_keys);
	style_ptr->property_keys = NULL;
	g_strfreev(style_ptr->property_values);
	style_ptr->property_values = NULL;
}


static void get_keyfile_keywords(GKeyFile *config, GKeyFile *configh,
				const gchar *key, guint ft_id, guint pos)
{
	style_sets[ft_id].keywords[pos] =
		utils_get_setting(string, configh, config, "keywords", key, "");
}


static void get_keyfile_wordchars(GKeyFile *config, GKeyFile *configh, gchar **wordchars)
{
	*wordchars = utils_get_setting(string, configh, config,
		"settings", "wordchars", GEANY_WORDCHARS);
}


static gboolean read_named_style(const gchar *named_style, GeanyLexerStyle *style)
{
	GeanyLexerStyle *cs;
	gchar *comma, *name = NULL;
	const gchar *bold = NULL;
	const gchar *italic = NULL;

	g_return_val_if_fail(named_style, FALSE);
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
	}
	return (cs != NULL);
}


/* Parses a color in `str` which can be an HTML color (ex. #0099cc),
 * an abbreviated HTML color (ex. #09c) or a hex string color
 * (ex. 0x0099cc). The result of the conversion is stored into the
 * location pointed to by `clr`. */
static void parse_color(GKeyFile *kf, const gchar *str, gint *clr)
{
	gint c;
	gchar hex_clr[9] = { 0 };
	gchar *named_color = NULL;
	const gchar *start;

	g_return_if_fail(clr != NULL);

	if (G_UNLIKELY(! NZV(str)))
		return;

	named_color = g_key_file_get_string(kf, "named_colors", str, NULL);
	if  (named_color)
		str = named_color;

	if (str[0] == '#')
		start = str + 1;
	else if (strlen(str) > 1 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
		start = str + 2;
	else
	{
		geany_debug("Bad color '%s'", str);
		g_free(named_color);
		return;
	}

	if (strlen(start) == 3)
	{
		snprintf(hex_clr, 9, "0x%c%c%c%c%c%c", start[0], start[0],
			start[1], start[1], start[2], start[2]);
	}
	else
		snprintf(hex_clr, 9, "0x%s", start);

	g_free(named_color);

	c = utils_strtod(hex_clr, NULL, FALSE);

	if (c > -1)
	{
		*clr = c;
		return;
	}
	geany_debug("Bad color '%s'", str);
}


static void parse_keyfile_style(GKeyFile *kf, gchar **list,
		const GeanyLexerStyle *default_style, GeanyLexerStyle *style)
{
	gsize len;

	g_return_if_fail(default_style);
	g_return_if_fail(style);

	*style = *default_style;

	if (!list)
		return;

	len = g_strv_length(list);
	if (len == 0)
		return;
	else if (len == 1)
	{
		gchar **items = g_strsplit(list[0], ",", 0);
		if (items != NULL)
		{
			if (g_strv_length(items) > 0)
			{
				if (g_hash_table_lookup(named_style_hash, items[0]) != NULL)
				{
					if (!read_named_style(list[0], style))
						geany_debug("Unable to read named style '%s'", items[0]);
					g_strfreev(items);
					return;
				}
				else if (strchr(list[0], ',') != NULL)
				{
					geany_debug("Unknown named style '%s'", items[0]);
					g_strfreev(items);
					return;
				}
			}
			g_strfreev(items);
		}
	}

	switch (len)
	{
		case 4:
			style->italic = utils_atob(list[3]);
		case 3:
			style->bold = utils_atob(list[2]);
		case 2:
			parse_color(kf, list[1], &style->background);
		case 1:
			parse_color(kf, list[0], &style->foreground);
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
	{
		list = g_key_file_get_string_list(config, "styling", key_name, &len, NULL);
		parse_keyfile_style(config, list, &gsd_default, style);
	}
	else
		parse_keyfile_style(configh, list, &gsd_default, style);

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


static void set_sci_style(ScintillaObject *sci, guint style, guint ft_id, guint styling_index)
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
	return utils_get_setting(string, configh, config,
		"settings", "whitespace_chars", GEANY_WHITESPACE_CHARS);
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

		parse_keyfile_style(config, list, &gsd_default, style);
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


static GKeyFile *utils_key_file_new(const gchar *filename)
{
	GKeyFile *config = g_key_file_new();

	g_key_file_load_from_file(config, filename, G_KEY_FILE_KEEP_COMMENTS, NULL);
	return config;
}


static void load_named_styles(GKeyFile *config, GKeyFile *config_home)
{
	const gchar *scheme = editor_prefs.color_scheme;
	gboolean free_kf = FALSE;

	if (named_style_hash)
		g_hash_table_destroy(named_style_hash);	/* reloading */

	named_style_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	if (NZV(scheme))
	{
		gchar *path, *path_home;

		path = g_build_path(G_DIR_SEPARATOR_S, app->datadir, GEANY_COLORSCHEMES_SUBDIR, scheme, NULL);
		path_home = g_build_path(G_DIR_SEPARATOR_S, app->configdir, GEANY_COLORSCHEMES_SUBDIR, scheme, NULL);

		if (g_file_test(path, G_FILE_TEST_EXISTS) || g_file_test(path_home, G_FILE_TEST_EXISTS))
		{
			config = utils_key_file_new(path);
			config_home = utils_key_file_new(path_home);
			free_kf = TRUE;
		}
		/* if color scheme is missing, use default */
		g_free(path);
		g_free(path_home);
	}
	/* first set default to the "default" named style */
	add_named_style(config, "default");
	read_named_style("default", &gsd_default);	/* in case user overrides but not with both colors */
	add_named_style(config_home, "default");
	read_named_style("default", &gsd_default);

	get_named_styles(config);
	/* home overrides any system named style */
	get_named_styles(config_home);

	if (free_kf)
	{
		g_key_file_free(config);
		g_key_file_free(config_home);
	}
}


static void styleset_common_init(GKeyFile *config, GKeyFile *config_home)
{
	load_named_styles(config, config_home);

	get_keyfile_style(config, config_home, "default", &common_style_set.styling[GCS_DEFAULT]);
	get_keyfile_style(config, config_home, "selection", &common_style_set.styling[GCS_SELECTION]);
	get_keyfile_style(config, config_home, "brace_good", &common_style_set.styling[GCS_BRACE_GOOD]);
	get_keyfile_style(config, config_home, "brace_bad", &common_style_set.styling[GCS_BRACE_BAD]);
	get_keyfile_style(config, config_home, "margin_linenumber", &common_style_set.styling[GCS_MARGIN_LINENUMBER]);
	get_keyfile_style(config, config_home, "margin_folding", &common_style_set.styling[GCS_MARGIN_FOLDING]);
	get_keyfile_style(config, config_home, "fold_symbol_highlight", &common_style_set.styling[GCS_FOLD_SYMBOL_HIGHLIGHT]);
	get_keyfile_style(config, config_home, "current_line", &common_style_set.styling[GCS_CURRENT_LINE]);
	get_keyfile_style(config, config_home, "caret", &common_style_set.styling[GCS_CARET]);
	get_keyfile_style(config, config_home, "indent_guide", &common_style_set.styling[GCS_INDENT_GUIDE]);
	get_keyfile_style(config, config_home, "white_space", &common_style_set.styling[GCS_WHITE_SPACE]);
	get_keyfile_style(config, config_home, "marker_line", &common_style_set.styling[GCS_MARKER_LINE]);
	get_keyfile_style(config, config_home, "marker_search", &common_style_set.styling[GCS_MARKER_SEARCH]);
	get_keyfile_style(config, config_home, "marker_mark", &common_style_set.styling[GCS_MARKER_MARK]);
	get_keyfile_style(config, config_home, "calltips", &common_style_set.styling[GCS_CALLTIPS]);

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


static void styleset_common(ScintillaObject *sci, guint ft_id)
{
	GeanyLexerStyle *style;

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
	SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEREND, SC_MARK_EMPTY);
	SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID, SC_MARK_EMPTY);
	switch (common_style_set.fold_marker)
	{
		case 2:
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_CIRCLEMINUS);
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_CIRCLEPLUS);
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEREND, SC_MARK_CIRCLEPLUSCONNECTED);
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID, SC_MARK_CIRCLEMINUSCONNECTED);
			break;
		default:
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_BOXMINUS);
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_BOXPLUS);
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEREND, SC_MARK_BOXPLUSCONNECTED);
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPENMID, SC_MARK_BOXMINUSCONNECTED);
			break;
		case 3:
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_ARROWDOWN);
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_ARROW);
			break;
		case 4:
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDEROPEN, SC_MARK_MINUS);
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDER, SC_MARK_PLUS);
			break;
	}

	/* choose the folding style - straight or curved, I prefer straight, so it is default ;-) */
	switch (common_style_set.fold_lines)
	{
		case 2:
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNERCURVE);
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNERCURVE);
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);
			break;
		default:
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_TCORNER);
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERTAIL, SC_MARK_LCORNER);
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERSUB, SC_MARK_VLINE);
			break;
		case 0:
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDERMIDTAIL, SC_MARK_EMPTY);
			SSM(sci, SCI_MARKERDEFINE, SC_MARKNUM_FOLDERTAIL, SC_MARK_EMPTY);
			SSM(sci, SCI_MARKERDEFINE,  SC_MARKNUM_FOLDERSUB, SC_MARK_EMPTY);
			break;
	}
	{
		gint markers[] = {
			SC_MARKNUM_FOLDEROPEN,
			SC_MARKNUM_FOLDER,
			SC_MARKNUM_FOLDERSUB,
			SC_MARKNUM_FOLDERTAIL,
			SC_MARKNUM_FOLDEREND,
			SC_MARKNUM_FOLDEROPENMID,
			SC_MARKNUM_FOLDERMIDTAIL
		};
		guint i;

		foreach_range(i, G_N_ELEMENTS(markers))
		{
			SSM(sci, SCI_MARKERSETFORE, markers[i],
				invert(common_style_set.styling[GCS_FOLD_SYMBOL_HIGHLIGHT].foreground));
			SSM(sci, SCI_MARKERSETBACK, markers[i],
				invert(common_style_set.styling[GCS_MARGIN_FOLDING].foreground));
		}
	}

	/* set some common defaults */
	sci_set_property(sci, "fold", "1");
	sci_set_property(sci, "fold.compact", "0");
	sci_set_property(sci, "fold.comment", "1");
	sci_set_property(sci, "fold.preprocessor", "1");
	sci_set_property(sci, "fold.at.else", "1");

	style = &common_style_set.styling[GCS_SELECTION];
	if (!style->bold && !style->italic)
	{
		geany_debug("selection style is set to invisible - ignoring!");
		style->italic = TRUE;
		style->background = 0xc0c0c0;
	}
	/* bold (3rd argument) is whether to override default foreground selection */
	SSM(sci, SCI_SETSELFORE, style->bold, invert(style->foreground));
	/* italic (4th argument) is whether to override default background selection */
	SSM(sci, SCI_SETSELBACK, style->italic, invert(style->background));

	SSM(sci, SCI_SETSTYLEBITS, SSM(sci, SCI_GETSTYLEBITSNEEDED, 0, 0), 0);

	SSM(sci, SCI_SETFOLDMARGINCOLOUR, 1, invert(common_style_set.styling[GCS_MARGIN_FOLDING].background));
	SSM(sci, SCI_SETFOLDMARGINHICOLOUR, 1, invert(common_style_set.styling[GCS_MARGIN_FOLDING].background));
	set_sci_style(sci, STYLE_LINENUMBER, GEANY_FILETYPES_NONE, GCS_MARGIN_LINENUMBER);
	set_sci_style(sci, STYLE_BRACELIGHT, GEANY_FILETYPES_NONE, GCS_BRACE_GOOD);
	set_sci_style(sci, STYLE_BRACEBAD, GEANY_FILETYPES_NONE, GCS_BRACE_BAD);
	set_sci_style(sci, STYLE_INDENTGUIDE, GEANY_FILETYPES_NONE, GCS_INDENT_GUIDE);

	/* bold = common whitespace settings enabled */
	SSM(sci, SCI_SETWHITESPACEFORE, common_style_set.styling[GCS_WHITE_SPACE].bold,
		invert(common_style_set.styling[GCS_WHITE_SPACE].foreground));
	SSM(sci, SCI_SETWHITESPACEBACK, common_style_set.styling[GCS_WHITE_SPACE].italic,
		invert(common_style_set.styling[GCS_WHITE_SPACE].background));

	if (common_style_set.styling[GCS_CALLTIPS].bold)
		SSM(sci, SCI_CALLTIPSETFORE, invert(common_style_set.styling[GCS_CALLTIPS].foreground), 1);
	if (common_style_set.styling[GCS_CALLTIPS].italic)
		SSM(sci, SCI_CALLTIPSETBACK, invert(common_style_set.styling[GCS_CALLTIPS].background), 1);
}


/* Merge & assign global typedefs and user secondary keywords.
 * keyword_idx is used for both style_sets[].keywords and scintilla keyword style number */
static void merge_type_keywords(ScintillaObject *sci, guint ft_id, guint keyword_idx)
{
	const gchar *user_words = style_sets[ft_id].keywords[keyword_idx];
	GString *s;

	s = get_global_typenames(filetypes[ft_id]->lang);
	if (G_UNLIKELY(s == NULL))
		s = g_string_sized_new(200);
	else
		g_string_append_c(s, ' '); /* append a space as delimiter to the existing list of words */

	g_string_append(s, user_words);

	sci_set_keywords(sci, keyword_idx, s->str);
	g_string_free(s, TRUE);
}


static void styleset_init_from_mapping(guint ft_id, GKeyFile *config, GKeyFile *config_home,
		const HLStyle *styles, gsize n_styles,
		const HLKeyword *keywords, gsize n_keywords)
{
	gsize i;

	/* styles */
	new_styleset(ft_id, n_styles);
	foreach_range(i, n_styles)
	{
		GeanyLexerStyle *style = &style_sets[ft_id].styling[i];

		get_keyfile_style(config, config_home, styles[i].name, style);
	}

	/* keywords */
	if (n_keywords < 1)
		style_sets[ft_id].keywords = NULL;
	else
	{
		style_sets[ft_id].keywords = g_new(gchar*, n_keywords + 1);
		foreach_range(i, n_keywords)
			get_keyfile_keywords(config, config_home, keywords[i].key, ft_id, i);
		style_sets[ft_id].keywords[i] = NULL;
	}
}


/* STYLE_DEFAULT will be set to match the first style. */
static void styleset_from_mapping(ScintillaObject *sci, guint ft_id, guint lexer,
		const HLStyle *styles, gsize n_styles,
		const HLKeyword *keywords, gsize n_keywords,
		const HLProperty *properties, gsize n_properties)
{
	gsize i;

	g_assert(ft_id != GEANY_FILETYPES_NONE);

	/* lexer */
	sci_set_lexer(sci, lexer);

	/* styles */
	styleset_common(sci, ft_id);
	if (n_styles > 0)
	{
		/* first style is also default one */
		set_sci_style(sci, STYLE_DEFAULT, ft_id, 0);
		foreach_range(i, n_styles)
		{
			if (styles[i].fill_eol)
				SSM(sci, SCI_STYLESETEOLFILLED, styles[i].style, TRUE);
			set_sci_style(sci, styles[i].style, ft_id, i);
		}
	}

	/* keywords */
	foreach_range(i, n_keywords)
	{
		if (keywords[i].merge)
			merge_type_keywords(sci, ft_id, i);
		else
			sci_set_keywords(sci, keywords[i].id, style_sets[ft_id].keywords[i]);
	}

	/* properties */
	foreach_range(i, n_properties)
		sci_set_property(sci, properties[i].property, properties[i].value);
}



static void styleset_default(ScintillaObject *sci, guint ft_id)
{
	sci_set_lexer(sci, SCLEX_NULL);

	/* we need to set STYLE_DEFAULT before we call SCI_STYLECLEARALL in styleset_common() */
	set_sci_style(sci, STYLE_DEFAULT, GEANY_FILETYPES_NONE, GCS_DEFAULT);

	styleset_common(sci, ft_id);
}


static void get_key_values(GKeyFile *config, const gchar *group, gchar **keys, gchar **values)
{
	while (*keys)
	{
		gchar *str = g_key_file_get_string(config, group, *keys, NULL);

		if (str)
			SETPTR(*values, str);

		keys++;
		values++;
	}
}


static void read_properties(GeanyFiletype *ft, GKeyFile *config, GKeyFile *configh)
{
	gchar group[] = "lexer_properties";
	gchar **keys;
	gchar **keysh = g_key_file_get_keys(configh, group, NULL, NULL);
	gchar **ptr;

	/* remove overridden keys from system keyfile */
	foreach_strv(ptr, keysh)
		g_key_file_remove_key(config, group, *ptr, NULL);

	/* merge sys and user keys */
	keys = g_key_file_get_keys(config, group, NULL, NULL);
	keys = utils_strv_join(keys, keysh);

	if (keys)
	{
		gchar **values = g_new0(gchar*, g_strv_length(keys) + 1);

		style_sets[ft->id].property_keys = keys;
		style_sets[ft->id].property_values = values;

		get_key_values(config, group, keys, values);
		get_key_values(configh, group, keys, values);
	}
}


static guint get_lexer_filetype(GeanyFiletype *ft)
{
	ft = NVL(ft->lexer_filetype, ft);
	return ft->id;
}


#define init_styleset_case(LANG_NAME) \
	case (GEANY_FILETYPES_##LANG_NAME): \
		styleset_init_from_mapping(filetype_idx, config, configh, \
				highlighting_styles_##LANG_NAME, \
				HL_N_ENTRIES(highlighting_styles_##LANG_NAME), \
				highlighting_keywords_##LANG_NAME, \
				HL_N_ENTRIES(highlighting_keywords_##LANG_NAME)); \
		break

/* Called by filetypes_load_config(). */
void highlighting_init_styles(guint filetype_idx, GKeyFile *config, GKeyFile *configh)
{
	GeanyFiletype *ft = filetypes[filetype_idx];
	guint lexer_id = get_lexer_filetype(ft);
	gchar *default_str;

	if (!style_sets)
		style_sets = g_new0(StyleSet, filetypes_array->len);

	/* Clear old information if necessary - e.g. when reloading config */
	free_styleset(filetype_idx);

	read_properties(ft, config, configh);
	/* If a default style exists, check it uses a named style
	 * Note: almost all filetypes have a "default" style, except HTML ones */
	default_str = utils_get_setting(string, configh, config,
		"styling", "default", "default");
	ft->priv->warn_color_scheme = !g_ascii_isalpha(*default_str);
	g_free(default_str);

	/* None filetype handled specially */
	if (filetype_idx == GEANY_FILETYPES_NONE)
	{
		styleset_common_init(config, configh);
		return;
	}
	/* All stylesets depend on filetypes.common */
	filetypes_load_config(GEANY_FILETYPES_NONE, FALSE);

	switch (lexer_id)
	{
		init_styleset_case(ADA);
		init_styleset_case(ASM);
		init_styleset_case(BASIC);
		init_styleset_case(C);
		init_styleset_case(CAML);
		init_styleset_case(CMAKE);
		init_styleset_case(COBOL);
		init_styleset_case(CONF);
		init_styleset_case(CSS);
		init_styleset_case(D);
		init_styleset_case(DIFF);
		init_styleset_case(LISP);
		init_styleset_case(ERLANG);
		init_styleset_case(DOCBOOK);
		init_styleset_case(FERITE);
		init_styleset_case(F77);
		init_styleset_case(FORTH);
		init_styleset_case(FORTRAN);
		init_styleset_case(HASKELL);
		init_styleset_case(HAXE);
		init_styleset_case(AS);
		init_styleset_case(HTML);
		init_styleset_case(JAVA);
		init_styleset_case(JS);
		init_styleset_case(LATEX);
		init_styleset_case(LUA);
		init_styleset_case(MAKE);
		init_styleset_case(MATLAB);
		init_styleset_case(MARKDOWN);
		init_styleset_case(NSIS);
		init_styleset_case(OBJECTIVEC);
		init_styleset_case(PASCAL);
		init_styleset_case(PERL);
		init_styleset_case(PHP);
		init_styleset_case(PO);
		init_styleset_case(PYTHON);
		init_styleset_case(R);
		init_styleset_case(RUBY);
		init_styleset_case(SH);
		init_styleset_case(SQL);
		init_styleset_case(TCL);
		init_styleset_case(TXT2TAGS);
		init_styleset_case(VHDL);
		init_styleset_case(VERILOG);
		init_styleset_case(XML);
		init_styleset_case(YAML);
		default:
			if (ft->lexer_filetype)
				geany_debug("Filetype %s has a recursive lexer_filetype %s set!",
					ft->name, ft->lexer_filetype->name);
	}

	/* should be done in filetypes.c really: */
	get_keyfile_wordchars(config, configh, &style_sets[filetype_idx].wordchars);
}


#define styleset_case(LANG_NAME) \
	case (GEANY_FILETYPES_##LANG_NAME): \
		styleset_from_mapping(sci, ft->id, highlighting_lexer_##LANG_NAME, \
				highlighting_styles_##LANG_NAME, \
				HL_N_ENTRIES(highlighting_styles_##LANG_NAME), \
				highlighting_keywords_##LANG_NAME, \
				HL_N_ENTRIES(highlighting_keywords_##LANG_NAME), \
				highlighting_properties_##LANG_NAME, \
				HL_N_ENTRIES(highlighting_properties_##LANG_NAME)); \
		break

/** Sets up highlighting and other visual settings.
 * @param sci Scintilla widget.
 * @param ft Filetype settings to use. */
void highlighting_set_styles(ScintillaObject *sci, GeanyFiletype *ft)
{
	guint lexer_id = get_lexer_filetype(ft);

	filetypes_load_config(ft->id, FALSE);	/* load filetypes.ext */

	switch (lexer_id)
	{
		styleset_case(ADA);
		styleset_case(ASM);
		styleset_case(BASIC);
		styleset_case(C);
		styleset_case(CAML);
		styleset_case(CMAKE);
		styleset_case(COBOL);
		styleset_case(CONF);
		styleset_case(CSS);
		styleset_case(D);
		styleset_case(DIFF);
		styleset_case(LISP);
		styleset_case(ERLANG);
		styleset_case(DOCBOOK);
		styleset_case(FERITE);
		styleset_case(F77);
		styleset_case(FORTH);
		styleset_case(FORTRAN);
		styleset_case(HASKELL);
		styleset_case(HAXE);
		styleset_case(AS);
		styleset_case(HTML);
		styleset_case(JAVA);
		styleset_case(JS);
		styleset_case(LATEX);
		styleset_case(LUA);
		styleset_case(MAKE);
		styleset_case(MARKDOWN);
		styleset_case(MATLAB);
		styleset_case(NSIS);
		styleset_case(OBJECTIVEC);
		styleset_case(PASCAL);
		styleset_case(PERL);
		styleset_case(PHP);
		styleset_case(PO);
		styleset_case(PYTHON);
		styleset_case(R);
		styleset_case(RUBY);
		styleset_case(SH);
		styleset_case(SQL);
		styleset_case(TCL);
		styleset_case(TXT2TAGS);
		styleset_case(VHDL);
		styleset_case(VERILOG);
		styleset_case(XML);
		styleset_case(YAML);
		case GEANY_FILETYPES_NONE:
		default:
			styleset_default(sci, ft->id);
	}
	/* [lexer_properties] settings */
	if (style_sets[ft->id].property_keys)
	{
		gchar **prop = style_sets[ft->id].property_keys;
		gchar **val = style_sets[ft->id].property_values;

		while (*prop)
		{
			sci_set_property(sci, *prop, *val);
			prop++;
			val++;
		}
	}
}


/** Retrieves a style @a style_id for the filetype @a ft_id.
 * If the style was not already initialised
 * (e.g. by by opening a file of this type), it will be initialised. The returned pointer is
 * owned by Geany and must not be freed.
 * @param ft_id Filetype ID, e.g. @c GEANY_FILETYPES_DIFF.
 * @param style_id A Scintilla lexer style, e.g. @c SCE_DIFF_ADDED. See scintilla/include/SciLexer.h.
 * @return A pointer to the style struct.
 * @see Scintilla messages @c SCI_STYLEGETFORE, etc, for use with scintilla_send_message(). */
const GeanyLexerStyle *highlighting_get_style(gint ft_id, gint style_id)
{
	g_return_val_if_fail(ft_id >= 0 && (guint) ft_id < filetypes_array->len, NULL);
	g_return_val_if_fail(style_id >= 0, NULL);

	/* ensure filetype loaded */
	filetypes_load_config((guint) ft_id, FALSE);

	/* TODO: style_id might not be the real array index (Scintilla styles are not always synced
	 * with array indices) */
	return get_style((guint) ft_id, (guint) style_id);
}


static GtkWidget *scheme_tree = NULL;

enum
{
	SCHEME_MARKUP,
	SCHEME_FILE,
	SCHEME_COLUMNS
};

static void on_color_scheme_changed(GtkTreeSelection *treesel, gpointer dummy)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *fname;
	gchar *path;

	if (!gtk_tree_selection_get_selected(treesel, &model, &iter))
		return;
	gtk_tree_model_get(model, &iter, SCHEME_FILE, &fname, -1);

	/* check if default item */
	if (!fname)
	{
		SETPTR(editor_prefs.color_scheme, NULL);
		filetypes_reload();
		return;
	}
	SETPTR(fname, utils_get_locale_from_utf8(fname));

	/* fname is just the basename from the menu item, so prepend the custom files path */
	path = g_build_path(G_DIR_SEPARATOR_S, app->configdir, GEANY_COLORSCHEMES_SUBDIR, fname, NULL);
	if (!g_file_test(path, G_FILE_TEST_EXISTS))
	{
		/* try the system path */
		g_free(path);
		path = g_build_path(G_DIR_SEPARATOR_S, app->datadir, GEANY_COLORSCHEMES_SUBDIR, fname, NULL);
	}
	if (g_file_test(path, G_FILE_TEST_EXISTS))
	{
		SETPTR(editor_prefs.color_scheme, fname);
		fname = NULL;
		filetypes_reload();
	}
	else
	{
		SETPTR(fname, utils_get_utf8_from_locale(fname));
		ui_set_statusbar(TRUE, _("Could not find file '%s'."), fname);
	}
	g_free(path);
	g_free(fname);
}


static gchar *utils_get_setting_locale_string(GKeyFile *keyfile,
		const gchar *group, const gchar *key, const gchar *default_value)
{
	gchar *result = g_key_file_get_locale_string(keyfile, group, key, NULL, NULL);

	return NVL(result, g_strdup(default_value));
}


static void add_color_scheme_item(GtkListStore *store,
	gchar *name, gchar *desc, const gchar *fn)
{
	GtkTreeIter iter;
	gchar *markup;

	/* reuse parameters */
	name = g_markup_escape_text(name, -1);
	desc = g_markup_escape_text(desc, -1);
	markup = g_strdup_printf("<big><b>%s</b></big>\n%s", name, desc);
	g_free(name);
	g_free(desc);

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, SCHEME_MARKUP, markup,
		SCHEME_FILE, fn, -1);
	g_free(markup);

	if (utils_str_equal(fn, editor_prefs.color_scheme))
	{
		GtkTreeSelection *treesel =
			gtk_tree_view_get_selection(GTK_TREE_VIEW(scheme_tree));

		gtk_tree_selection_select_iter(treesel, &iter);
	}
}


static void add_color_scheme_file(GtkListStore *store, const gchar *fname)
{
	GKeyFile *hkeyfile, *skeyfile;
	gchar *path, *theme_name, *theme_desc;
	gchar *theme_fn = utils_get_utf8_from_locale(fname);

	path = g_build_filename(app->configdir, GEANY_COLORSCHEMES_SUBDIR, fname, NULL);
	hkeyfile = utils_key_file_new(path);
	SETPTR(path, g_build_filename(app->datadir, GEANY_COLORSCHEMES_SUBDIR, fname, NULL));
	skeyfile = utils_key_file_new(path);

	theme_name = utils_get_setting(locale_string, hkeyfile, skeyfile, "theme_info", "name", theme_fn);
	theme_desc = utils_get_setting(locale_string, hkeyfile, skeyfile, "theme_info", "description", NULL);
	add_color_scheme_item(store, theme_name, theme_desc, theme_fn);

	g_free(path);
	g_free(theme_fn);
	g_free(theme_name);
	g_free(theme_desc);
	g_key_file_free(hkeyfile);
	g_key_file_free(skeyfile);
}


static gboolean add_color_scheme_items(GtkListStore *store)
{
	GSList *list, *node;

	add_color_scheme_item(store, _("Default"), _("Default"), NULL);
	list = utils_get_config_files(GEANY_COLORSCHEMES_SUBDIR);

	foreach_slist(node, list)
	{
		gchar *fname = node->data;

		if (g_str_has_suffix(fname, ".conf"))
			add_color_scheme_file(store, fname);

		g_free(fname);
	}
	g_slist_free(list);
	return list != NULL;
}


static void on_color_scheme_dialog_response(GtkWidget *dialog,
	gint response, gpointer *dialog_ptr)
{
	*dialog_ptr = NULL;
	gtk_widget_destroy(dialog);
}


void highlighting_show_color_scheme_dialog(void)
{
	static GtkWidget *dialog = NULL;
	GtkListStore *store = gtk_list_store_new(SCHEME_COLUMNS,
		G_TYPE_STRING, G_TYPE_STRING);
	GtkCellRenderer *text_renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *treesel;
	GtkWidget *vbox, *swin, *tree;
	GeanyDocument *doc;

	doc = document_get_current();
	if (doc && doc->file_type->priv->warn_color_scheme)
		dialogs_show_msgbox_with_secondary(GTK_MESSAGE_WARNING,
			_("The current filetype overrides the default style."),
			_("This may cause color schemes to display incorrectly."));

	scheme_tree = tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);

	text_renderer = gtk_cell_renderer_text_new();
	g_object_set(text_renderer, "wrap-mode", PANGO_WRAP_WORD, NULL);
	column = gtk_tree_view_column_new_with_attributes(
		NULL, text_renderer, "markup", SCHEME_MARKUP, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	add_color_scheme_items(store);

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	g_signal_connect(treesel, "changed", G_CALLBACK(on_color_scheme_changed), NULL);

	/* old dialog may still be showing */
	if (dialog)
		gtk_widget_destroy(dialog);
	dialog = gtk_dialog_new_with_buttons(_("Color Schemes"),
		GTK_WINDOW(main_widgets.window), GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 6);
	gtk_widget_set_name(dialog, "GeanyDialog");
	gtk_window_set_default_size(GTK_WINDOW(dialog),
		GEANY_DEFAULT_DIALOG_HEIGHT * 7/4, GEANY_DEFAULT_DIALOG_HEIGHT);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(swin), tree);
	gtk_container_add(GTK_CONTAINER(vbox), swin);
	g_signal_connect(dialog, "response", G_CALLBACK(on_color_scheme_dialog_response), &dialog);
	gtk_widget_show_all(dialog);
}


/** Checks whether the given style is a string for the given lexer.
 *
 * @param lexer Scintilla lexer type (@c SCLEX_*).
 * @param style Scintilla style (@c SCE_*).
 *
 * @return @c TRUE if the style is a string, @c FALSE otherwise.
 */
gboolean highlighting_is_string_style(gint lexer, gint style)
{
	/* Don't forget STRINGEOL, to prevent completion whilst typing a string with no closing char. */

	switch (lexer)
	{
		case SCLEX_CPP:
			return (style == SCE_C_CHARACTER ||
				style == SCE_C_STRING ||
				style == SCE_C_STRINGEOL ||
				style == SCE_C_STRINGRAW ||
				style == SCE_C_VERBATIM ||
				style == SCE_C_TRIPLEVERBATIM);

		case SCLEX_PASCAL:
			return (style == SCE_PAS_CHARACTER ||
				style == SCE_PAS_STRING ||
				style == SCE_PAS_STRINGEOL);

		case SCLEX_D:
			return (style == SCE_D_STRING ||
				style == SCE_D_STRINGEOL ||
				style == SCE_D_CHARACTER ||
				style == SCE_D_STRINGB ||
				style == SCE_D_STRINGR);

		case SCLEX_PYTHON:
			return (style == SCE_P_STRING ||
				style == SCE_P_TRIPLE ||
				style == SCE_P_TRIPLEDOUBLE ||
				style == SCE_P_CHARACTER ||
				style == SCE_P_STRINGEOL);

		case SCLEX_F77:
		case SCLEX_FORTRAN:
			return (style == SCE_F_STRING1 ||
				style == SCE_F_STRING2 ||
				style == SCE_F_STRINGEOL);

		case SCLEX_PERL:
			return (style == SCE_PL_STRING ||
				style == SCE_PL_CHARACTER ||
				style == SCE_PL_HERE_DELIM ||
				style == SCE_PL_HERE_Q ||
				style == SCE_PL_HERE_QQ ||
				style == SCE_PL_HERE_QX ||
				style == SCE_PL_POD ||
				style == SCE_PL_STRING_Q ||
				style == SCE_PL_STRING_QQ ||
				style == SCE_PL_STRING_QX ||
				style == SCE_PL_STRING_QR ||
				style == SCE_PL_STRING_QW ||
				style == SCE_PL_POD_VERB ||
				style == SCE_PL_XLAT
				/* we don't include any STRING_*_VAR for autocompletion */);

		case SCLEX_R:
			return (style == SCE_R_STRING);

		case SCLEX_RUBY:
			return (style == SCE_RB_CHARACTER ||
				style == SCE_RB_STRING ||
				style == SCE_RB_HERE_DELIM ||
				style == SCE_RB_HERE_Q ||
				style == SCE_RB_HERE_QQ ||
				style == SCE_RB_HERE_QX ||
				style == SCE_RB_POD);

		case SCLEX_BASH:
			return (style == SCE_SH_STRING);

		case SCLEX_SQL:
			return (style == SCE_SQL_STRING);

		case SCLEX_TCL:
			return (style == SCE_TCL_IN_QUOTE);

		case SCLEX_LUA:
			return (style == SCE_LUA_LITERALSTRING ||
				style == SCE_LUA_CHARACTER ||
				style == SCE_LUA_STRINGEOL ||
				style == SCE_LUA_STRING);

		case SCLEX_HASKELL:
			return (style == SCE_HA_CHARACTER ||
				style == SCE_HA_STRING);

		case SCLEX_FREEBASIC:
			return (style == SCE_B_STRING ||
				style == SCE_B_STRINGEOL);

		case SCLEX_OCTAVE:
			return (style == SCE_MATLAB_STRING ||
				style == SCE_MATLAB_DOUBLEQUOTESTRING);

		case SCLEX_XML:
		case SCLEX_HTML:
			return (
				style == SCE_HBA_STRING ||
				style == SCE_HBA_STRINGEOL ||
				style == SCE_HB_STRING ||
				style == SCE_HB_STRINGEOL ||
				style == SCE_H_CDATA ||
				style == SCE_H_DOUBLESTRING ||
				style == SCE_HJA_DOUBLESTRING ||
				style == SCE_HJA_SINGLESTRING ||
				style == SCE_HJA_STRINGEOL ||
				style == SCE_HJ_DOUBLESTRING ||
				style == SCE_HJ_SINGLESTRING ||
				style == SCE_HJ_STRINGEOL ||
				style == SCE_HPA_CHARACTER ||
				style == SCE_HPA_STRING ||
				style == SCE_HPA_TRIPLE ||
				style == SCE_HPA_TRIPLEDOUBLE ||
				style == SCE_HP_CHARACTER ||
				style == SCE_HPHP_HSTRING ||  /* HSTRING is a heredoc */
				style == SCE_HPHP_HSTRING_VARIABLE ||
				style == SCE_HPHP_SIMPLESTRING ||
				style == SCE_HP_STRING ||
				style == SCE_HP_TRIPLE ||
				style == SCE_HP_TRIPLEDOUBLE ||
				style == SCE_H_SGML_DOUBLESTRING ||
				style == SCE_H_SGML_SIMPLESTRING ||
				style == SCE_H_SINGLESTRING);

		case SCLEX_CMAKE:
			return (style == SCE_CMAKE_STRINGDQ ||
				style == SCE_CMAKE_STRINGLQ ||
				style == SCE_CMAKE_STRINGRQ ||
				style == SCE_CMAKE_STRINGVAR);

		case SCLEX_NSIS:
			return (style == SCE_NSIS_STRINGDQ ||
				style == SCE_NSIS_STRINGLQ ||
				style == SCE_NSIS_STRINGRQ ||
				style == SCE_NSIS_STRINGVAR);

		case SCLEX_ADA:
			return (style == SCE_ADA_CHARACTER ||
				style == SCE_ADA_STRING ||
				style == SCE_ADA_CHARACTEREOL ||
				style == SCE_ADA_STRINGEOL);
	}
	return FALSE;
}


/** Checks whether the given style is a comment for the given lexer.
 *
 * @param lexer Scintilla lexer type (@c SCLEX_*).
 * @param style Scintilla style (@c SCE_*).
 *
 * @return @c TRUE if the style is a comment, @c FALSE otherwise.
 */
gboolean highlighting_is_comment_style(gint lexer, gint style)
{
	switch (lexer)
	{
		case SCLEX_COBOL:
		case SCLEX_CPP:
			return (style == SCE_C_COMMENT ||
				style == SCE_C_COMMENTLINE ||
				style == SCE_C_COMMENTDOC ||
				style == SCE_C_COMMENTLINEDOC ||
				style == SCE_C_COMMENTDOCKEYWORD ||
				style == SCE_C_COMMENTDOCKEYWORDERROR);

		case SCLEX_PASCAL:
			return (style == SCE_PAS_COMMENT ||
				style == SCE_PAS_COMMENT2 ||
				style == SCE_PAS_COMMENTLINE);

		case SCLEX_D:
			return (style == SCE_D_COMMENT ||
				style == SCE_D_COMMENTLINE ||
				style == SCE_D_COMMENTDOC ||
				style == SCE_D_COMMENTNESTED ||
				style == SCE_D_COMMENTLINEDOC ||
				style == SCE_D_COMMENTDOCKEYWORD ||
				style == SCE_D_COMMENTDOCKEYWORDERROR);

		case SCLEX_PYTHON:
			return (style == SCE_P_COMMENTLINE ||
				style == SCE_P_COMMENTBLOCK);

		case SCLEX_F77:
		case SCLEX_FORTRAN:
			return (style == SCE_F_COMMENT);

		case SCLEX_PERL:
			return (style == SCE_PL_COMMENTLINE);

		case SCLEX_PROPERTIES:
			return (style == SCE_PROPS_COMMENT);

		case SCLEX_PO:
			return (style == SCE_PO_COMMENT);

		case SCLEX_LATEX:
			return (style == SCE_L_COMMENT ||
				style == SCE_L_COMMENT2);

		case SCLEX_MAKEFILE:
			return (style == SCE_MAKE_COMMENT);

		case SCLEX_RUBY:
			return (style == SCE_RB_COMMENTLINE);

		case SCLEX_BASH:
			return (style == SCE_SH_COMMENTLINE);

		case SCLEX_R:
			return (style == SCE_R_COMMENT);

		case SCLEX_SQL:
			return (style == SCE_SQL_COMMENT ||
				style == SCE_SQL_COMMENTLINE ||
				style == SCE_SQL_COMMENTDOC ||
				style == SCE_SQL_COMMENTLINEDOC ||
				style == SCE_SQL_COMMENTDOCKEYWORD ||
				style == SCE_SQL_COMMENTDOCKEYWORDERROR);

		case SCLEX_TCL:
			return (style == SCE_TCL_COMMENT ||
				style == SCE_TCL_COMMENTLINE ||
				style == SCE_TCL_COMMENT_BOX ||
				style == SCE_TCL_BLOCK_COMMENT);

		case SCLEX_OCTAVE:
			return (style == SCE_MATLAB_COMMENT);

		case SCLEX_LUA:
			return (style == SCE_LUA_COMMENT ||
				style == SCE_LUA_COMMENTLINE ||
				style == SCE_LUA_COMMENTDOC);

		case SCLEX_HASKELL:
			return (style == SCE_HA_COMMENTLINE ||
				style == SCE_HA_COMMENTBLOCK ||
				style == SCE_HA_COMMENTBLOCK2 ||
				style == SCE_HA_COMMENTBLOCK3);

		case SCLEX_FREEBASIC:
			return (style == SCE_B_COMMENT);

		case SCLEX_YAML:
			return (style == SCE_YAML_COMMENT);

		case SCLEX_XML:
		case SCLEX_HTML:
			return (
				style == SCE_HBA_COMMENTLINE ||
				style == SCE_HB_COMMENTLINE ||
				style == SCE_H_COMMENT ||
				style == SCE_HJA_COMMENT ||
				style == SCE_HJA_COMMENTDOC ||
				style == SCE_HJA_COMMENTLINE ||
				style == SCE_HJ_COMMENT ||
				style == SCE_HJ_COMMENTDOC ||
				style == SCE_HJ_COMMENTLINE ||
				style == SCE_HPA_COMMENTLINE ||
				style == SCE_HP_COMMENTLINE ||
				style == SCE_HPHP_COMMENT ||
				style == SCE_HPHP_COMMENTLINE ||
				style == SCE_H_SGML_COMMENT);

		case SCLEX_CMAKE:
			return (style == SCE_CMAKE_COMMENT);

		case SCLEX_NSIS:
			return (style == SCE_NSIS_COMMENT ||
				style == SCE_NSIS_COMMENTBOX);

		case SCLEX_ADA:
			return (style == SCE_ADA_COMMENTLINE ||
				style == SCE_NSIS_COMMENTBOX);

		case SCLEX_ASM:
			return (style == SCE_ASM_COMMENT ||
				style == SCE_ASM_COMMENTBLOCK ||
				style == SCE_ASM_COMMENTDIRECTIVE);
	}
	return FALSE;
}


/** Checks whether the given style is normal code (not string, comment, preprocessor, etc).
 *
 * @param lexer Scintilla lexer type (@c SCLEX_*).
 * @param style Scintilla style (@c SCE_*).
 *
 * @return @c TRUE if the style is code, @c FALSE otherwise.
 */
gboolean highlighting_is_code_style(gint lexer, gint style)
{
	switch (lexer)
	{
		case SCLEX_CPP:
			if (style == SCE_C_PREPROCESSOR)
				return FALSE;
			break;
	}
	return !(highlighting_is_comment_style(lexer, style) ||
		highlighting_is_string_style(lexer, style));
}
