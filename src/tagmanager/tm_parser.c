/*
 *      tm_parser.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2016 Jiri Techet <techet(at)gmail(dot)com>
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "tm_parser.h"
#include "tm_ctags_wrappers.h"

#include <string.h>


typedef struct
{
    const gchar kind;
    TMTagType type;
} TMParserMapEntry;


static TMParserMapEntry map_C[] = {
	{'c', tm_tag_class_t},
	{'d', tm_tag_macro_t},
	{'e', tm_tag_enumerator_t},
	{'f', tm_tag_function_t},
	{'g', tm_tag_enum_t},
	{'m', tm_tag_member_t},
	{'n', tm_tag_namespace_t},
	{'p', tm_tag_prototype_t},
	{'s', tm_tag_struct_t},
	{'t', tm_tag_typedef_t},
	{'u', tm_tag_union_t},
	{'v', tm_tag_variable_t},
	{'x', tm_tag_externvar_t},
};

/* C++, same as C */
#define map_CPP map_C

static TMParserMapEntry map_JAVA[] = {
	{'c', tm_tag_class_t},
	{'f', tm_tag_field_t},
	{'i', tm_tag_interface_t},
	{'m', tm_tag_method_t},
	{'p', tm_tag_package_t},
	{'e', tm_tag_enumerator_t},
	{'g', tm_tag_enum_t},
};

static TMParserMapEntry map_MAKEFILE[] = {
	{'m', tm_tag_macro_t},
	{'t', tm_tag_function_t},
};

static TMParserMapEntry map_PASCAL[] = {
	{'f', tm_tag_function_t},
	{'p', tm_tag_function_t},
};

static TMParserMapEntry map_PERL[] = {
	{'c', tm_tag_enum_t},
	{'f', tm_tag_other_t},
	{'l', tm_tag_macro_t},
	{'p', tm_tag_package_t},
	{'s', tm_tag_function_t},
	{'d', tm_tag_prototype_t},
};

static TMParserMapEntry map_PHP[] = {
	{'c', tm_tag_class_t},
	{'d', tm_tag_macro_t},
	{'f', tm_tag_function_t},
	{'i', tm_tag_interface_t},
	{'l', tm_tag_undef_t},
	{'n', tm_tag_namespace_t},
	{'t', tm_tag_struct_t},
	{'v', tm_tag_variable_t},
};

static TMParserMapEntry map_PYTHON[] = {
	{'c', tm_tag_class_t},
	{'f', tm_tag_function_t},
	{'m', tm_tag_method_t},
	{'v', tm_tag_variable_t},
    /* defined as externvar to get those excluded as forward type in symbols.c:goto_tag()
     * so we can jump to the real implementation (if known) instead of to the import statement */
	{'x', tm_tag_externvar_t},
};

/* different parser than tex.c from universal-ctags */
static TMParserMapEntry map_LATEX[] = {
	{'f', tm_tag_function_t},
	{'c', tm_tag_class_t},
	{'m', tm_tag_member_t},
	{'d', tm_tag_macro_t},
	{'v', tm_tag_variable_t},
	{'n', tm_tag_namespace_t},
	{'s', tm_tag_struct_t},
};

static TMParserMapEntry map_ASM[] = {
	{'d', tm_tag_macro_t},
	{'l', tm_tag_namespace_t},
	{'m', tm_tag_function_t},
	{'t', tm_tag_struct_t},
};

/* not in universal-ctags */
static TMParserMapEntry map_CONF[] = {
	{'n', tm_tag_namespace_t},
	{'m', tm_tag_macro_t},
};

static TMParserMapEntry map_SQL[] = {
	{'c', tm_tag_undef_t},
	{'d', tm_tag_prototype_t},
	{'f', tm_tag_function_t},
	{'F', tm_tag_field_t},
	{'l', tm_tag_undef_t},
	{'L', tm_tag_undef_t},
	{'P', tm_tag_package_t},
	{'p', tm_tag_namespace_t},
	{'r', tm_tag_undef_t},
	{'s', tm_tag_undef_t},
	{'t', tm_tag_class_t},
	{'T', tm_tag_macro_t},
	{'v', tm_tag_variable_t},
	{'i', tm_tag_struct_t},
	{'e', tm_tag_undef_t},
	{'U', tm_tag_undef_t},
	{'R', tm_tag_undef_t},
	{'D', tm_tag_undef_t},
	{'V', tm_tag_member_t},
	{'n', tm_tag_undef_t},
	{'x', tm_tag_undef_t},
	{'y', tm_tag_undef_t},
	{'z', tm_tag_undef_t},
};

/* not in universal-ctags */
static TMParserMapEntry map_DOCBOOK[] = {
	{'f', tm_tag_function_t},
	{'c', tm_tag_class_t},
	{'m', tm_tag_member_t},
	{'d', tm_tag_macro_t},
	{'v', tm_tag_variable_t},
	{'s', tm_tag_struct_t},
};

static TMParserMapEntry map_ERLANG[] = {
	{'d', tm_tag_macro_t},
	{'f', tm_tag_function_t},
	{'m', tm_tag_undef_t},
	{'r', tm_tag_struct_t},
	{'t', tm_tag_typedef_t},
};

static TMParserMapEntry map_CSS[] = {
	{'c', tm_tag_class_t},
	{'s', tm_tag_struct_t},
	{'i', tm_tag_variable_t},
};

static TMParserMapEntry map_RUBY[] = {
	{'c', tm_tag_class_t},
	{'f', tm_tag_method_t},
	{'m', tm_tag_namespace_t},
	{'F', tm_tag_member_t},
};

static TMParserMapEntry map_TCL[] = {
	{'c', tm_tag_class_t},
	{'m', tm_tag_member_t},
	{'p', tm_tag_function_t},
	{'n', tm_tag_namespace_t},
};

static TMParserMapEntry map_SH[] = {
	{'f', tm_tag_function_t},
};

static TMParserMapEntry map_D[] = {
	{'c', tm_tag_class_t},
	{'e', tm_tag_enumerator_t},
	{'f', tm_tag_function_t},
	{'g', tm_tag_enum_t},
	{'i', tm_tag_interface_t},
	{'m', tm_tag_member_t},
	{'n', tm_tag_namespace_t},
	{'p', tm_tag_prototype_t},
	{'s', tm_tag_struct_t},
	{'t', tm_tag_typedef_t},
	{'u', tm_tag_union_t},
	{'v', tm_tag_variable_t},
	{'x', tm_tag_externvar_t},
};

static TMParserMapEntry map_DIFF[] = {
	{'f', tm_tag_function_t},
};

/* different parser than in universal-ctags */
static TMParserMapEntry map_VHDL[] = {
	{'c', tm_tag_variable_t},
	{'t', tm_tag_typedef_t},
	{'v', tm_tag_variable_t},
	{'a', tm_tag_undef_t},
	{'s', tm_tag_variable_t},
	{'f', tm_tag_function_t},
	{'p', tm_tag_function_t},
	{'k', tm_tag_member_t},
	{'l', tm_tag_namespace_t},
	{'m', tm_tag_member_t},
	{'n', tm_tag_class_t},
	{'o', tm_tag_struct_t},
	{'u', tm_tag_undef_t},
	{'b', tm_tag_member_t},
	{'A', tm_tag_typedef_t},
};

static TMParserMapEntry map_LUA[] = {
	{'f', tm_tag_function_t},
};

static TMParserMapEntry map_JAVASCRIPT[] = {
	{'f', tm_tag_function_t},
	{'c', tm_tag_class_t},
	{'m', tm_tag_method_t},
	{'p', tm_tag_member_t},
	{'C', tm_tag_macro_t},
	{'v', tm_tag_variable_t},
};

/* not in universal-ctags */
static TMParserMapEntry map_HASKELL[] = {
	{'t', tm_tag_typedef_t},
	{'c', tm_tag_macro_t},
	{'f', tm_tag_function_t},
	{'m', tm_tag_namespace_t},
};

static TMParserMapEntry map_CSHARP[] = {
	{'c', tm_tag_class_t},
	{'d', tm_tag_macro_t},
	{'e', tm_tag_enumerator_t},
	{'E', tm_tag_undef_t},
	{'f', tm_tag_field_t},
	{'g', tm_tag_enum_t},
	{'i', tm_tag_interface_t},
	{'l', tm_tag_undef_t},
	{'m', tm_tag_method_t},
	{'n', tm_tag_namespace_t},
	{'p', tm_tag_undef_t},
	{'s', tm_tag_struct_t},
	{'t', tm_tag_typedef_t},
};

static TMParserMapEntry map_FREEBASIC[] = {
	{'c', tm_tag_macro_t},
	{'f', tm_tag_function_t},
	{'l', tm_tag_namespace_t},
	{'t', tm_tag_struct_t},
	{'v', tm_tag_variable_t},
	{'g', tm_tag_externvar_t},
};

/* not in universal-ctags */
static TMParserMapEntry map_HAXE[] = {
	{'m', tm_tag_method_t},
	{'c', tm_tag_class_t},
	{'e', tm_tag_enum_t},
	{'v', tm_tag_variable_t},
	{'i', tm_tag_interface_t},
	{'t', tm_tag_typedef_t},
};

/* not in universal-ctags */
static TMParserMapEntry map_REST[] = {
	{'n', tm_tag_namespace_t},
	{'m', tm_tag_member_t},
	{'d', tm_tag_macro_t},
	{'v', tm_tag_variable_t},
};

static TMParserMapEntry map_HTML[] = {
	{'a', tm_tag_member_t},
	{'f', tm_tag_function_t},
	{'n', tm_tag_namespace_t},
	{'c', tm_tag_class_t},
	{'v', tm_tag_variable_t},
};

static TMParserMapEntry map_F77[] = {
	{'b', tm_tag_undef_t},
	{'c', tm_tag_macro_t},
	{'e', tm_tag_undef_t},
	{'f', tm_tag_function_t},
	{'i', tm_tag_interface_t},
	{'k', tm_tag_member_t},
	{'l', tm_tag_undef_t},
	{'L', tm_tag_undef_t},
	{'m', tm_tag_namespace_t},
	{'n', tm_tag_undef_t},
	{'p', tm_tag_struct_t},
	{'s', tm_tag_method_t},
	{'t', tm_tag_class_t},
	{'v', tm_tag_variable_t},
	{'E', tm_tag_enum_t},
	{'N', tm_tag_enumerator_t},
};

#define map_FORTRAN map_F77

#define map_FERITE map_C

/* different parser than in universal-ctags */
static TMParserMapEntry map_MATLAB[] = {
	{'f', tm_tag_function_t},
	{'s', tm_tag_struct_t},
};

#define map_GLSL map_C

/* not in universal-ctags */
static TMParserMapEntry map_VALA[] = {
	{'c', tm_tag_class_t},
	{'d', tm_tag_macro_t},
	{'e', tm_tag_enumerator_t},
	{'f', tm_tag_field_t},
	{'g', tm_tag_enum_t},
	{'i', tm_tag_interface_t},
	{'l', tm_tag_undef_t},
	{'m', tm_tag_method_t},
	{'n', tm_tag_namespace_t},
	{'p', tm_tag_undef_t},
	{'S', tm_tag_undef_t},
	{'s', tm_tag_struct_t},
};

/* not in universal-ctags */
static TMParserMapEntry map_ACTIONSCRIPT[] = {
	{'f', tm_tag_function_t},
	{'l', tm_tag_field_t},
	{'v', tm_tag_variable_t},
	{'m', tm_tag_macro_t},
	{'c', tm_tag_class_t},
	{'i', tm_tag_interface_t},
	{'p', tm_tag_package_t},
	{'o', tm_tag_other_t},
	{'r', tm_tag_prototype_t},
};

/* not in universal-ctags */
static TMParserMapEntry map_NSIS[] = {
	{'n', tm_tag_namespace_t},
	{'f', tm_tag_function_t},
	{'v', tm_tag_variable_t},
};

/* not in universal-ctags */
static TMParserMapEntry map_MARKDOWN[] = {
	{'v', tm_tag_variable_t},
};

/* not in universal-ctags */
static TMParserMapEntry map_TXT2TAGS[] = {
	{'m', tm_tag_member_t},
};

/* not in universal-ctags */
static TMParserMapEntry map_ABC[] = {
	{'m', tm_tag_member_t},
	{'s', tm_tag_struct_t},
};

static TMParserMapEntry map_VERILOG[] = {
	{'c', tm_tag_variable_t},
	{'e', tm_tag_typedef_t},
	{'f', tm_tag_function_t},
	{'m', tm_tag_class_t},
	{'n', tm_tag_variable_t},
	{'p', tm_tag_variable_t},
	{'r', tm_tag_variable_t},
	{'t', tm_tag_function_t},
};

static TMParserMapEntry map_R[] = {
	{'f', tm_tag_function_t},
	{'l', tm_tag_other_t},
	{'s', tm_tag_other_t},
};

static TMParserMapEntry map_COBOL[] = {
	{'d', tm_tag_variable_t},
	{'f', tm_tag_function_t},
	{'g', tm_tag_struct_t},
	{'p', tm_tag_macro_t},
	{'P', tm_tag_class_t},
	{'s', tm_tag_namespace_t},
};

static TMParserMapEntry map_OBJC[] = {
	{'i', tm_tag_interface_t},
	{'I', tm_tag_undef_t},
	{'P', tm_tag_undef_t},
	{'m', tm_tag_method_t},
	{'c', tm_tag_class_t},
	{'v', tm_tag_variable_t},
	{'F', tm_tag_field_t},
	{'f', tm_tag_function_t},
	{'p', tm_tag_undef_t},
	{'t', tm_tag_typedef_t},
	{'s', tm_tag_struct_t},
	{'e', tm_tag_enum_t},
	{'M', tm_tag_macro_t},
};

/* not in universal-ctags */
static TMParserMapEntry map_ASCIIDOC[] = {
	{'n', tm_tag_namespace_t},
	{'m', tm_tag_member_t},
	{'d', tm_tag_macro_t},
	{'v', tm_tag_variable_t},
	{'s', tm_tag_struct_t},
};

/* not in universal-ctags */
static TMParserMapEntry map_ABAQUS[] = {
	{'c', tm_tag_class_t},
	{'m', tm_tag_member_t},
	{'n', tm_tag_interface_t},
};

static TMParserMapEntry map_RUST[] = {
	{'n', tm_tag_namespace_t},
	{'s', tm_tag_struct_t},
	{'i', tm_tag_interface_t},
	{'c', tm_tag_class_t},
	{'f', tm_tag_function_t},
	{'g', tm_tag_enum_t},
	{'t', tm_tag_typedef_t},
	{'v', tm_tag_variable_t},
	{'M', tm_tag_macro_t},
	{'m', tm_tag_field_t},
	{'e', tm_tag_enumerator_t},
	{'F', tm_tag_method_t},
};

static TMParserMapEntry map_GO[] = {
	{'p', tm_tag_namespace_t},
	{'f', tm_tag_function_t},
	{'c', tm_tag_macro_t},
	{'t', tm_tag_typedef_t},
	{'v', tm_tag_variable_t},
	{'s', tm_tag_struct_t},
	{'i', tm_tag_interface_t},
	{'m', tm_tag_member_t},
};

static TMParserMapEntry map_JSON[] = {
	{'o', tm_tag_member_t},
	{'a', tm_tag_member_t},
	{'n', tm_tag_member_t},
	{'s', tm_tag_member_t},
	{'b', tm_tag_member_t},
	{'z', tm_tag_member_t},
};

/* Zephir, same as PHP */
#define map_ZEPHIR map_PHP

/* not in universal-ctags */
static TMParserMapEntry map_POWERSHELL[] = {
	{'f', tm_tag_function_t},
	{'v', tm_tag_variable_t},
};


typedef struct
{
    TMParserMapEntry *entries;
    guint size;
} TMParserMap;

#define MAP_ENTRY(lang) [TM_PARSER_##lang] = {map_##lang, G_N_ELEMENTS(map_##lang)}

/* keep in sync with TM_PARSER_* definitions in the header */
static TMParserMap parser_map[] = {
	MAP_ENTRY(C),
	MAP_ENTRY(CPP),
	MAP_ENTRY(JAVA),
	MAP_ENTRY(MAKEFILE),
	MAP_ENTRY(PASCAL),
	MAP_ENTRY(PERL),
	MAP_ENTRY(PHP),
	MAP_ENTRY(PYTHON),
	MAP_ENTRY(LATEX),
	MAP_ENTRY(ASM),
	MAP_ENTRY(CONF),
	MAP_ENTRY(SQL),
	MAP_ENTRY(DOCBOOK),
	MAP_ENTRY(ERLANG),
	MAP_ENTRY(CSS),
	MAP_ENTRY(RUBY),
	MAP_ENTRY(TCL),
	MAP_ENTRY(SH),
	MAP_ENTRY(D),
	MAP_ENTRY(FORTRAN),
	MAP_ENTRY(FERITE),
	MAP_ENTRY(DIFF),
	MAP_ENTRY(VHDL),
	MAP_ENTRY(LUA),
	MAP_ENTRY(JAVASCRIPT),
	MAP_ENTRY(HASKELL),
	MAP_ENTRY(CSHARP),
	MAP_ENTRY(FREEBASIC),
	MAP_ENTRY(HAXE),
	MAP_ENTRY(REST),
	MAP_ENTRY(HTML),
	MAP_ENTRY(F77),
	MAP_ENTRY(GLSL),
	MAP_ENTRY(MATLAB),
	MAP_ENTRY(VALA),
	MAP_ENTRY(ACTIONSCRIPT),
	MAP_ENTRY(NSIS),
	MAP_ENTRY(MARKDOWN),
	MAP_ENTRY(TXT2TAGS),
	MAP_ENTRY(ABC),
	MAP_ENTRY(VERILOG),
	MAP_ENTRY(R),
	MAP_ENTRY(COBOL),
	MAP_ENTRY(OBJC),
	MAP_ENTRY(ASCIIDOC),
	MAP_ENTRY(ABAQUS),
	MAP_ENTRY(RUST),
	MAP_ENTRY(GO),
	MAP_ENTRY(JSON),
	MAP_ENTRY(ZEPHIR),
	MAP_ENTRY(POWERSHELL),
};
/* make sure the parser map is consistent and complete */
G_STATIC_ASSERT(G_N_ELEMENTS(parser_map) == TM_PARSER_COUNT);


TMTagType tm_parser_get_tag_type(gchar kind, TMParserType lang)
{
	TMParserMap *map = &parser_map[lang];
	guint i;

	for (i = 0; i < map->size; i++)
	{
		TMParserMapEntry *entry = &map->entries[i];

		if (entry->kind == kind)
			return entry->type;
	}
	return tm_tag_undef_t;
}


gchar tm_parser_get_tag_kind(TMTagType type, TMParserType lang)
{
	TMParserMap *map = &parser_map[lang];
	guint i;

	for (i = 0; i < map->size; i++)
	{
		TMParserMapEntry *entry = &map->entries[i];

		if (entry->type == type)
			return entry->kind;
	}
	return '\0';
}


void tm_parser_verify_type_mappings(void)
{
	TMParserType lang;

	if (TM_PARSER_COUNT > tm_ctags_get_lang_count())
		g_error("More parsers defined in Geany than in ctags");

	for (lang = 0; lang < TM_PARSER_COUNT; lang++)
	{
		const gchar *kinds = tm_ctags_get_lang_kinds(lang);
		TMParserMap *map = &parser_map[lang];
		gchar presence_map[256];
		guint i;

		if (! map->entries || map->size < 1)
			g_error("No tag types in TM for %s, is the language listed in parser_map?",
					tm_ctags_get_lang_name(lang));

		/* TODO: check also regex parser mappings. At the moment there's no way
		 * to access regex parser definitions in ctags */
		if (tm_ctags_is_using_regex_parser(lang))
			continue;

		if (map->size != strlen(kinds))
			g_error("Different number of tag types in TM (%d) and ctags (%d) for %s",
				map->size, (int)strlen(kinds), tm_ctags_get_lang_name(lang));

		memset(presence_map, 0, sizeof(presence_map));
		for (i = 0; i < map->size; i++)
		{
			gboolean ctags_found = FALSE;
			gboolean tm_found = FALSE;
			guint j;

			for (j = 0; j < map->size; j++)
			{
				/* check that for every type in TM there's a type in ctags */
				if (map->entries[i].kind == kinds[j])
					ctags_found = TRUE;
				/* check that for every type in ctags there's a type in TM */
				if (map->entries[j].kind == kinds[i])
					tm_found = TRUE;
				if (ctags_found && tm_found)
					break;
			}
			if (!ctags_found)
				g_error("Tag type '%c' found in TM but not in ctags for %s",
					map->entries[i].kind, tm_ctags_get_lang_name(lang));
			if (!tm_found)
				g_error("Tag type '%c' found in ctags but not in TM for %s",
					kinds[i], tm_ctags_get_lang_name(lang));

			presence_map[(unsigned char) map->entries[i].kind]++;
		}

		for (i = 0; i < sizeof(presence_map); i++)
		{
			if (presence_map[i] > 1)
				g_error("Duplicate tag type '%c' found for %s",
					(gchar)i, tm_ctags_get_lang_name(lang));
		}
	}
}


const gchar *tm_parser_context_separator(TMParserType lang)
{
	switch (lang)
	{
		case TM_PARSER_C:	/* for C++ .h headers or C structs */
		case TM_PARSER_CPP:
		case TM_PARSER_GLSL:	/* for structs */
		/*case GEANY_FILETYPES_RUBY:*/ /* not sure what to use atm*/
		case TM_PARSER_PHP:
		case TM_PARSER_POWERSHELL:
		case TM_PARSER_RUST:
		case TM_PARSER_ZEPHIR:
			return "::";

		/* avoid confusion with other possible separators in group/section name */
		case TM_PARSER_CONF:
		case TM_PARSER_REST:
			return ":::";

		/* no context separator */
		case TM_PARSER_ASCIIDOC:
		case TM_PARSER_TXT2TAGS:
			return "\x03";

		default:
			return ".";
	}
}


gboolean tm_parser_has_full_context(TMParserType lang)
{
	switch (lang)
	{
		/* These parsers include full hierarchy in the tag scope, separated by tm_parser_context_separator() */
		case TM_PARSER_C:
		case TM_PARSER_CPP:
		case TM_PARSER_CSHARP:
		case TM_PARSER_D:
		case TM_PARSER_FERITE:
		case TM_PARSER_GLSL:
		case TM_PARSER_JAVA:
		case TM_PARSER_JAVASCRIPT:
		case TM_PARSER_JSON:
		case TM_PARSER_PHP:
		case TM_PARSER_POWERSHELL:
		case TM_PARSER_PYTHON:
		case TM_PARSER_RUBY:
		case TM_PARSER_RUST:
		case TM_PARSER_SQL:
		case TM_PARSER_TXT2TAGS:
		case TM_PARSER_VALA:
		case TM_PARSER_ZEPHIR:
			return TRUE;

		/* These make use of the scope, but don't include nested hierarchy
		 * (either as a parser limitation or a language semantic) */
		case TM_PARSER_ASCIIDOC:
		case TM_PARSER_CONF:
		case TM_PARSER_ERLANG:
		case TM_PARSER_F77:
		case TM_PARSER_FORTRAN:
		case TM_PARSER_GO:
		case TM_PARSER_OBJC:
		case TM_PARSER_REST:
		/* Other parsers don't use scope at all (or should be somewhere above) */
		default:
			return FALSE;
	}
}


gboolean tm_parser_langs_compatible(TMParserType lang, TMParserType other)
{
	if (lang == TM_PARSER_NONE || other == TM_PARSER_NONE)
		return FALSE;
	if (lang == other)
		return TRUE;
	/* Accept CPP tags for C lang and vice versa */
	else if (lang == TM_PARSER_C && other == TM_PARSER_CPP)
		return TRUE;
	else if (lang == TM_PARSER_CPP && other == TM_PARSER_C)
		return TRUE;

	return FALSE;
}
