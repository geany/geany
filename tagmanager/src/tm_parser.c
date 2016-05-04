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


static TMParserMapEntry c_map[] = {
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

static TMParserMapEntry java_map[] = {
	{'c', tm_tag_class_t},
	{'f', tm_tag_field_t},
	{'i', tm_tag_interface_t},
	{'m', tm_tag_method_t},
	{'p', tm_tag_package_t},
	{'e', tm_tag_enumerator_t},
	{'g', tm_tag_enum_t},
};

static TMParserMapEntry makefile_map[] = {
	{'m', tm_tag_macro_t},
	{'t', tm_tag_function_t},
};

static TMParserMapEntry pascal_map[] = {
	{'f', tm_tag_function_t},
	{'p', tm_tag_function_t},
};

static TMParserMapEntry perl_map[] = {
	{'c', tm_tag_enum_t},
	{'f', tm_tag_other_t},
	{'l', tm_tag_macro_t},
	{'p', tm_tag_package_t},
	{'s', tm_tag_function_t},
	{'d', tm_tag_prototype_t},
};

static TMParserMapEntry php_map[] = {
	{'c', tm_tag_class_t},
	{'d', tm_tag_macro_t},
	{'f', tm_tag_function_t},
	{'i', tm_tag_interface_t},
	{'l', tm_tag_undef_t},
	{'n', tm_tag_namespace_t},
	{'t', tm_tag_struct_t},
	{'v', tm_tag_variable_t},
};

static TMParserMapEntry python_map[] = {
	{'c', tm_tag_class_t},
	{'f', tm_tag_function_t},
	{'m', tm_tag_method_t},
	{'v', tm_tag_variable_t},
    /* defined as externvar to get those excluded as forward type in symbols.c:goto_tag()
     * so we can jump to the real implementation (if known) instead of to the import statement */
	{'x', tm_tag_externvar_t},
};

/* different parser than tex.c from universal-ctags */
static TMParserMapEntry latex_map[] = {
	{'f', tm_tag_function_t},
	{'c', tm_tag_class_t},
	{'m', tm_tag_member_t},
	{'d', tm_tag_macro_t},
	{'v', tm_tag_variable_t},
	{'n', tm_tag_namespace_t},
	{'s', tm_tag_struct_t},
};

static TMParserMapEntry asm_map[] = {
	{'d', tm_tag_macro_t},
	{'l', tm_tag_namespace_t},
	{'m', tm_tag_function_t},
	{'t', tm_tag_struct_t},
};

/* not in universal-ctags */
static TMParserMapEntry conf_map[] = {
	{'n', tm_tag_namespace_t},
	{'m', tm_tag_macro_t},
};

static TMParserMapEntry sql_map[] = {
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
static TMParserMapEntry docbook_map[] = {
	{'f', tm_tag_function_t},
	{'c', tm_tag_class_t},
	{'m', tm_tag_member_t},
	{'d', tm_tag_macro_t},
	{'v', tm_tag_variable_t},
	{'s', tm_tag_struct_t},
};

static TMParserMapEntry erlang_map[] = {
	{'d', tm_tag_macro_t},
	{'f', tm_tag_function_t},
	{'m', tm_tag_undef_t},
	{'r', tm_tag_struct_t},
	{'t', tm_tag_typedef_t},
};

static TMParserMapEntry css_map[] = {
	{'c', tm_tag_class_t},
	{'s', tm_tag_struct_t},
	{'i', tm_tag_variable_t},
};

static TMParserMapEntry ruby_map[] = {
	{'c', tm_tag_class_t},
	{'f', tm_tag_method_t},
	{'m', tm_tag_namespace_t},
	{'F', tm_tag_member_t},
};

static TMParserMapEntry tcl_map[] = {
	{'c', tm_tag_class_t},
	{'m', tm_tag_member_t},
	{'p', tm_tag_function_t},
	{'n', tm_tag_namespace_t},
};

static TMParserMapEntry sh_map[] = {
	{'f', tm_tag_function_t},
};

static TMParserMapEntry d_map[] = {
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

static TMParserMapEntry diff_map[] = {
	{'f', tm_tag_function_t},
};

/* different parser than in universal-ctags */
static TMParserMapEntry vhdl_map[] = {
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

static TMParserMapEntry lua_map[] = {
	{'f', tm_tag_function_t},
};

static TMParserMapEntry javascript_map[] = {
	{'f', tm_tag_function_t},
	{'c', tm_tag_class_t},
	{'m', tm_tag_method_t},
	{'p', tm_tag_member_t},
	{'C', tm_tag_macro_t},
	{'v', tm_tag_variable_t},
};

/* not in universal-ctags */
static TMParserMapEntry haskell_map[] = {
	{'t', tm_tag_typedef_t},
	{'c', tm_tag_macro_t},
	{'f', tm_tag_function_t},
	{'m', tm_tag_namespace_t},
};

static TMParserMapEntry csharp_map[] = {
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

static TMParserMapEntry freebasic_map[] = {
	{'c', tm_tag_macro_t},
	{'f', tm_tag_function_t},
	{'l', tm_tag_namespace_t},
	{'t', tm_tag_struct_t},
	{'v', tm_tag_variable_t},
	{'g', tm_tag_externvar_t},
};

/* not in universal-ctags */
static TMParserMapEntry haxe_map[] = {
	{'m', tm_tag_method_t},
	{'c', tm_tag_class_t},
	{'e', tm_tag_enum_t},
	{'v', tm_tag_variable_t},
	{'i', tm_tag_interface_t},
	{'t', tm_tag_typedef_t},
};

/* not in universal-ctags */
static TMParserMapEntry rest_map[] = {
	{'n', tm_tag_namespace_t},
	{'m', tm_tag_member_t},
	{'d', tm_tag_macro_t},
	{'v', tm_tag_variable_t},
};

static TMParserMapEntry html_map[] = {
	{'a', tm_tag_member_t},
	{'f', tm_tag_function_t},
	{'n', tm_tag_namespace_t},
	{'c', tm_tag_class_t},
	{'v', tm_tag_variable_t},
};

static TMParserMapEntry f77_map[] = {
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

/* different parser than in universal-ctags */
static TMParserMapEntry matlab_map[] = {
	{'f', tm_tag_function_t},
	{'s', tm_tag_struct_t},
};

/* not in universal-ctags */
static TMParserMapEntry vala_map[] = {
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
static TMParserMapEntry actionscript_map[] = {
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
static TMParserMapEntry nsis_map[] = {
	{'n', tm_tag_namespace_t},
	{'f', tm_tag_function_t},
	{'v', tm_tag_variable_t},
};

/* not in universal-ctags */
static TMParserMapEntry markdown_map[] = {
	{'v', tm_tag_variable_t},
};

/* not in universal-ctags */
static TMParserMapEntry txt2tags_map[] = {
	{'m', tm_tag_member_t},
};

/* not in universal-ctags */
static TMParserMapEntry abc_map[] = {
	{'m', tm_tag_member_t},
	{'s', tm_tag_struct_t},
};

static TMParserMapEntry verilog_map[] = {
	{'c', tm_tag_variable_t},
	{'e', tm_tag_typedef_t},
	{'f', tm_tag_function_t},
	{'m', tm_tag_class_t},
	{'n', tm_tag_variable_t},
	{'p', tm_tag_variable_t},
	{'r', tm_tag_variable_t},
	{'t', tm_tag_function_t},
};

static TMParserMapEntry r_map[] = {
	{'f', tm_tag_function_t},
	{'l', tm_tag_other_t},
	{'s', tm_tag_other_t},
};

static TMParserMapEntry cobol_map[] = {
	{'d', tm_tag_variable_t},
	{'f', tm_tag_function_t},
	{'g', tm_tag_struct_t},
	{'p', tm_tag_macro_t},
	{'P', tm_tag_class_t},
	{'s', tm_tag_namespace_t},
};

static TMParserMapEntry objc_map[] = {
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
static TMParserMapEntry asciidoc_map[] = {
	{'n', tm_tag_namespace_t},
	{'m', tm_tag_member_t},
	{'d', tm_tag_macro_t},
	{'v', tm_tag_variable_t},
	{'s', tm_tag_struct_t},
};

/* not in universal-ctags */
static TMParserMapEntry abaqus_map[] = {
	{'c', tm_tag_class_t},
	{'m', tm_tag_member_t},
	{'n', tm_tag_interface_t},
};

static TMParserMapEntry rust_map[] = {
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

static TMParserMapEntry go_map[] = {
	{'p', tm_tag_namespace_t},
	{'f', tm_tag_function_t},
	{'c', tm_tag_macro_t},
	{'t', tm_tag_typedef_t},
	{'v', tm_tag_variable_t},
	{'s', tm_tag_struct_t},
	{'i', tm_tag_interface_t},
	{'m', tm_tag_member_t},
};

static TMParserMapEntry json_map[] = {
	{'o', tm_tag_member_t},
	{'a', tm_tag_member_t},
	{'n', tm_tag_member_t},
	{'s', tm_tag_member_t},
	{'b', tm_tag_member_t},
	{'z', tm_tag_member_t},
};

/* not in universal-ctags */
static TMParserMapEntry powershell_map[] = {
	{'f', tm_tag_function_t},
	{'v', tm_tag_variable_t},
};


typedef struct
{
    TMParserMapEntry *entries;
    guint size;
} TMParserMap;

#define MAP_ENTRY(map) {map, (sizeof(map)/sizeof(TMParserMapEntry))}

/* keep in sync with TM_PARSER_* definitions in the header */
static TMParserMap parser_map[] = {
	MAP_ENTRY(c_map),
	MAP_ENTRY(c_map),	/* C++ - same as C */
	MAP_ENTRY(java_map),
	MAP_ENTRY(makefile_map),
	MAP_ENTRY(pascal_map),
	MAP_ENTRY(perl_map),
	MAP_ENTRY(php_map),
	MAP_ENTRY(python_map),
	MAP_ENTRY(latex_map),
	MAP_ENTRY(asm_map),
	MAP_ENTRY(conf_map),
	MAP_ENTRY(sql_map),
	MAP_ENTRY(docbook_map),
	MAP_ENTRY(erlang_map),
	MAP_ENTRY(css_map),
	MAP_ENTRY(ruby_map),
	MAP_ENTRY(tcl_map),
	MAP_ENTRY(sh_map),
	MAP_ENTRY(d_map),
	MAP_ENTRY(f77_map),
	MAP_ENTRY(c_map),	/* Ferite - same as C */
	MAP_ENTRY(diff_map),
	MAP_ENTRY(vhdl_map),
	MAP_ENTRY(lua_map),
	MAP_ENTRY(javascript_map),
	MAP_ENTRY(haskell_map),
	MAP_ENTRY(csharp_map),
	MAP_ENTRY(freebasic_map),
	MAP_ENTRY(haxe_map),
	MAP_ENTRY(rest_map),
	MAP_ENTRY(html_map),
	MAP_ENTRY(f77_map),
	MAP_ENTRY(c_map),	/* GLSL - same as C */
	MAP_ENTRY(matlab_map),
	MAP_ENTRY(vala_map),
	MAP_ENTRY(actionscript_map),
	MAP_ENTRY(nsis_map),
	MAP_ENTRY(markdown_map),
	MAP_ENTRY(txt2tags_map),
	MAP_ENTRY(abc_map),
	MAP_ENTRY(verilog_map),
	MAP_ENTRY(r_map),
	MAP_ENTRY(cobol_map),
	MAP_ENTRY(objc_map),
	MAP_ENTRY(asciidoc_map),
	MAP_ENTRY(abaqus_map),
	MAP_ENTRY(rust_map),
	MAP_ENTRY(go_map),
	MAP_ENTRY(json_map),
	MAP_ENTRY(php_map),	/* Zephir - same as PHP */
	MAP_ENTRY(powershell_map),
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
	return '-';
}


void tm_parser_verify_type_mappings(void)
{
	TMParserType lang;

	if (TM_PARSER_COUNT > tm_ctags_get_lang_count())
	{
		g_warning("More parsers defined in Geany than in ctags");
		return;
	}

	for (lang = 0; lang < TM_PARSER_COUNT; lang++)
	{
		const gchar *kinds = tm_ctags_get_lang_kinds(lang);
		TMParserMap *map = &parser_map[lang];
		gchar presence_map[256];
		guint i;

		/* TODO: check also regex parser mappings. At the moment there's no way
		 * to access regex parser definitions in ctags */
		if (tm_ctags_is_using_regex_parser(lang))
			continue;

		if (map->size != strlen(kinds))
		{
			g_warning("Different number of tag types in TM (%d) and ctags (%d) for %s",
				map->size, (int)strlen(kinds), tm_ctags_get_lang_name(lang));
			continue;
		}

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
				g_warning("Tag type '%c' found in TM but not in ctags for %s",
					map->entries[i].kind, tm_ctags_get_lang_name(lang));
			if (!tm_found)
				g_warning("Tag type '%c' found in ctags but not in TM for %s",
					kinds[i], tm_ctags_get_lang_name(lang));

			presence_map[(unsigned char) map->entries[i].kind]++;
		}

		for (i = 0; i < sizeof(presence_map); i++)
		{
			if (presence_map[i] > 1)
				g_warning("Duplicate tag type '%c' found for %s",
					(gchar)i, tm_ctags_get_lang_name(lang));
		}
	}
}
