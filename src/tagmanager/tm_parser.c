/*
 *      tm_parser.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2016 The Geany contributors
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
#include "tm_ctags.h"

#include <string.h>

#include "config.h"


/* Only for the command-line xgettext tool to find translatable strings.
 * The gettext() function is invoked manually using glib g_dgettext() */
#define _(String) (String)

typedef struct
{
    const gchar kind;
    TMTagType type;
} TMParserMapEntry;

/* Allows remapping a subparser tag type to another type if there's a clash with
 * the master parser tag type. Only subparser tag types explicitly listed within
 * TMSubparserMapEntry maps are added to tag manager - tags with types not listed
 * are discarded to prevent uncontrolled merging of tags from master parser and
 * subparsers. */
typedef struct
{
    TMTagType orig_type;
    TMTagType new_type;
} TMSubparserMapEntry;

typedef struct
{
    const gchar *name;
    guint icon;
    TMTagType  types;
} TMParserMapGroup;

static GHashTable *subparser_map = NULL;


#define COMMON_C \
	{'d', tm_tag_macro_t},       /* macro */      \
	{'e', tm_tag_enumerator_t},  /* enumerator */ \
	{'f', tm_tag_function_t},    /* function */   \
	{'g', tm_tag_enum_t},        /* enum */       \
	{'m', tm_tag_member_t},      /* member */     \
	{'p', tm_tag_prototype_t},   /* prototype */  \
	{'s', tm_tag_struct_t},      /* struct */     \
	{'t', tm_tag_typedef_t},     /* typedef */    \
	{'u', tm_tag_union_t},       /* union */      \
	{'v', tm_tag_variable_t},    /* variable */   \
	{'x', tm_tag_externvar_t},   /* externvar */  \
	{'h', tm_tag_include_t},     /* header */     \
	{'l', tm_tag_local_var_t},   /* local */      \
	{'z', tm_tag_local_var_t},   /* parameter */  \
	{'L', tm_tag_undef_t},       /* label */      \
	{'D', tm_tag_undef_t},       /* macroparam */

static TMParserMapEntry map_C[] = {
	COMMON_C
};
/* Used also by other languages than C - keep all the tm_tag_* here even though
 * they aren't used by C as they might be used by some other language */
static TMParserMapGroup group_C[] = {
	{_("Namespaces"), TM_ICON_NAMESPACE, tm_tag_namespace_t | tm_tag_package_t | tm_tag_include_t},
	{_("Classes"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Interfaces"), TM_ICON_STRUCT, tm_tag_interface_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_prototype_t | tm_tag_method_t | tm_tag_function_t},
	{_("Members"), TM_ICON_MEMBER, tm_tag_member_t | tm_tag_field_t},
	{_("Structs"), TM_ICON_STRUCT, tm_tag_union_t | tm_tag_struct_t},
	{_("Typedefs / Enums"), TM_ICON_STRUCT, tm_tag_typedef_t | tm_tag_enum_t},
	{_("Macros"), TM_ICON_MACRO, tm_tag_macro_t | tm_tag_macro_with_arg_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t | tm_tag_enumerator_t | tm_tag_local_var_t},
	{_("Extern Variables"), TM_ICON_VAR, tm_tag_externvar_t},
	{_("Other"), TM_ICON_OTHER, tm_tag_other_t},
};

static TMParserMapEntry map_CPP[] = {
	COMMON_C
	{'c', tm_tag_class_t},      // class
	{'n', tm_tag_namespace_t},  // namespace
	{'A', tm_tag_undef_t},      // alias
	{'N', tm_tag_undef_t},      // name
	{'U', tm_tag_undef_t},      // using
	{'Z', tm_tag_undef_t},      // tparam
};
#define group_CPP group_C

static TMParserMapEntry map_JAVA[] = {
	{'c', tm_tag_class_t},      // class
	{'f', tm_tag_field_t},      // field
	{'i', tm_tag_interface_t},  // interface
	{'m', tm_tag_method_t},     // method
	{'p', tm_tag_package_t},    // package
	{'e', tm_tag_enumerator_t}, // enumConstant
	{'g', tm_tag_enum_t},       // enum
};
static TMParserMapGroup group_JAVA[] = {
	{_("Package"), TM_ICON_NAMESPACE, tm_tag_package_t},
	{_("Interfaces"), TM_ICON_STRUCT, tm_tag_interface_t},
	{_("Classes"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Methods"), TM_ICON_METHOD, tm_tag_method_t},
	{_("Members"), TM_ICON_MEMBER, tm_tag_field_t},
	{_("Enums"), TM_ICON_STRUCT, tm_tag_enum_t},
	{_("Other"), TM_ICON_VAR, tm_tag_enumerator_t},
};

// no scope information
static TMParserMapEntry map_MAKEFILE[] = {
	{'m', tm_tag_macro_t},     // macro
	{'t', tm_tag_function_t},  // target
	{'I', tm_tag_undef_t},     // makefile
};
static TMParserMapGroup group_MAKEFILE[] = {
	{_("Targets"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Macros"), TM_ICON_MACRO, tm_tag_macro_t},
};

static TMParserMapEntry map_PASCAL[] = {
	{'f', tm_tag_function_t},  // function
	{'p', tm_tag_function_t},  // procedure
};
static TMParserMapGroup group_PASCAL[] = {
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
};

// no scope information
static TMParserMapEntry map_PERL[] = {
	{'c', tm_tag_enum_t},       // constant
	{'f', tm_tag_other_t},      // format
	{'l', tm_tag_macro_t},      // label
	{'p', tm_tag_package_t},    // package
	{'s', tm_tag_function_t},   // subroutine
	{'d', tm_tag_prototype_t},  // subroutineDeclaration
	{'M', tm_tag_undef_t},      // module
};
static TMParserMapGroup group_PERL[] = {
	{_("Package"), TM_ICON_NAMESPACE, tm_tag_package_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t | tm_tag_prototype_t},
	{_("Labels"), TM_ICON_NONE, tm_tag_macro_t},
	{_("Constants"), TM_ICON_NONE, tm_tag_enum_t},
	{_("Other"), TM_ICON_OTHER, tm_tag_other_t},
};

static TMParserMapEntry map_PHP[] = {
	{'c', tm_tag_class_t},      // class
	{'d', tm_tag_macro_t},      // define
	{'f', tm_tag_function_t},   // function
	{'i', tm_tag_interface_t},  // interface
	{'l', tm_tag_local_var_t},  // local
	{'n', tm_tag_namespace_t},  // namespace
	{'t', tm_tag_struct_t},     // trait
	{'v', tm_tag_variable_t},   // variable
	{'a', tm_tag_undef_t},      // alias
};
static TMParserMapGroup group_PHP[] = {
	{_("Namespaces"), TM_ICON_NAMESPACE, tm_tag_namespace_t},
	{_("Interfaces"), TM_ICON_STRUCT, tm_tag_interface_t},
	{_("Classes"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Constants"), TM_ICON_MACRO, tm_tag_macro_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t | tm_tag_local_var_t},
	{_("Traits"), TM_ICON_STRUCT, tm_tag_struct_t},
};

static TMParserMapEntry map_PYTHON[] = {
	{'c', tm_tag_class_t},      // class
	{'f', tm_tag_function_t},   // function
	{'m', tm_tag_method_t},     // member
	{'v', tm_tag_variable_t},   // variable
	{'I', tm_tag_externvar_t},  // namespace
	{'i', tm_tag_externvar_t},  // module
    /* defined as externvar to get those excluded as forward type in symbols.c:goto_tag()
     * so we can jump to the real implementation (if known) instead of to the import statement */
	{'x', tm_tag_externvar_t},  // unknown
	{'z', tm_tag_local_var_t},  // parameter
	{'l', tm_tag_local_var_t},  // local
};
static TMParserMapGroup group_PYTHON[] = {
	{_("Classes"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Methods"), TM_ICON_MACRO, tm_tag_method_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t | tm_tag_local_var_t},
	{_("Imports"), TM_ICON_NAMESPACE, tm_tag_externvar_t},
};

static TMParserMapEntry map_LATEX[] = {
	{'p', tm_tag_enum_t},       // part
	{'c', tm_tag_namespace_t},  // chapter
	{'s', tm_tag_member_t},     // section
	{'u', tm_tag_macro_t},      // subsection
	{'b', tm_tag_variable_t},   // subsubsection
	{'P', tm_tag_undef_t},      // paragraph
	{'G', tm_tag_undef_t},      // subparagraph
	{'l', tm_tag_struct_t},     // label
	{'i', tm_tag_undef_t},      // xinput
	{'B', tm_tag_field_t},      // bibitem
	{'C', tm_tag_function_t},   // command
	{'o', tm_tag_function_t},   // operator
	{'e', tm_tag_class_t},      // environment
	{'t', tm_tag_class_t},      // theorem
	{'N', tm_tag_undef_t},      // counter
};
static TMParserMapGroup group_LATEX[] = {
	{_("Command"), TM_ICON_NONE, tm_tag_function_t},
	{_("Environment"), TM_ICON_NONE, tm_tag_class_t},
	{_("Part"), TM_ICON_NONE, tm_tag_enum_t},
	{_("Chapter"), TM_ICON_NONE, tm_tag_namespace_t},
	{_("Section"), TM_ICON_NONE, tm_tag_member_t},
	{_("Subsection"), TM_ICON_NONE, tm_tag_macro_t},
	{_("Subsubsection"), TM_ICON_NONE, tm_tag_variable_t},
	{_("Bibitem"), TM_ICON_NONE, tm_tag_field_t},
	{_("Label"), TM_ICON_NONE, tm_tag_struct_t},
};

// no scope information
static TMParserMapEntry map_BIBTEX[] = {
	{'a', tm_tag_function_t},   // article
	{'b', tm_tag_class_t},      // book
	{'B', tm_tag_class_t},      // booklet
	{'c', tm_tag_member_t},     // conference
	{'i', tm_tag_macro_t},      // inbook
	{'I', tm_tag_macro_t},      // incollection
	{'j', tm_tag_member_t},     // inproceedings
	{'m', tm_tag_other_t},      // manual
	{'M', tm_tag_variable_t},   // mastersthesis
	{'n', tm_tag_other_t},      // misc
	{'p', tm_tag_variable_t},   // phdthesis
	{'P', tm_tag_class_t},      // proceedings
	{'s', tm_tag_namespace_t},  // string
	{'t', tm_tag_other_t},      // techreport
	{'u', tm_tag_externvar_t},  // unpublished
};
static TMParserMapGroup group_BIBTEX[] = {
	{_("Articles"), TM_ICON_NONE, tm_tag_function_t},
	{_("Book Chapters"), TM_ICON_NONE, tm_tag_macro_t},
	{_("Books & Conference Proceedings"), TM_ICON_NONE, tm_tag_class_t},
	{_("Conference Papers"), TM_ICON_NONE, tm_tag_member_t},
	{_("Theses"), TM_ICON_NONE, tm_tag_variable_t},
	{_("Strings"), TM_ICON_NONE, tm_tag_namespace_t},
	{_("Unpublished"), TM_ICON_NONE, tm_tag_externvar_t},
	{_("Other"), TM_ICON_NONE, tm_tag_other_t},
};

static TMParserMapEntry map_ASM[] = {
	{'d', tm_tag_macro_t},      // define
	{'l', tm_tag_namespace_t},  // label
	{'m', tm_tag_function_t},   // macro
	{'t', tm_tag_struct_t},     // type
	{'s', tm_tag_undef_t},      // section
};
static TMParserMapGroup group_ASM[] = {
	{_("Labels"), TM_ICON_NAMESPACE, tm_tag_namespace_t},
	{_("Macros"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Defines"), TM_ICON_MACRO, tm_tag_macro_t},
	{_("Types"), TM_ICON_STRUCT, tm_tag_struct_t},
};

static TMParserMapEntry map_CONF[] = {
	{'s', tm_tag_namespace_t},  // section
	{'k', tm_tag_macro_t},      // key
};
static TMParserMapGroup group_CONF[] = {
	{_("Sections"), TM_ICON_OTHER, tm_tag_namespace_t},
	{_("Keys"), TM_ICON_VAR, tm_tag_macro_t},
};

static TMParserMapEntry map_SQL[] = {
	{'c', tm_tag_undef_t},      // cursor
	{'d', tm_tag_prototype_t},  // prototype
	{'f', tm_tag_function_t},   // function
	{'E', tm_tag_field_t},      // field
	{'l', tm_tag_undef_t},      // local
	{'L', tm_tag_undef_t},      // label
	{'P', tm_tag_package_t},    // package
	{'p', tm_tag_namespace_t},  // procedure
	{'r', tm_tag_undef_t},      // record
	{'s', tm_tag_undef_t},      // subtype
	{'t', tm_tag_class_t},      // table
	{'T', tm_tag_macro_t},      // trigger
	{'v', tm_tag_variable_t},   // variable
	{'i', tm_tag_struct_t},     // index
	{'e', tm_tag_undef_t},      // event
	{'U', tm_tag_undef_t},      // publication
	{'R', tm_tag_undef_t},      // service
	{'D', tm_tag_undef_t},      // domain
	{'V', tm_tag_member_t},     // view
	{'n', tm_tag_undef_t},      // synonym
	{'x', tm_tag_undef_t},      // mltable
	{'y', tm_tag_undef_t},      // mlconn
	{'z', tm_tag_undef_t},      // mlprop
	{'C', tm_tag_undef_t},      // ccflag
};
static TMParserMapGroup group_SQL[] = {
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t | tm_tag_prototype_t},
	{_("Procedures"), TM_ICON_NAMESPACE, tm_tag_namespace_t | tm_tag_package_t},
	{_("Indexes"), TM_ICON_STRUCT, tm_tag_struct_t},
	{_("Tables"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Triggers"), TM_ICON_MACRO, tm_tag_macro_t},
	{_("Views"), TM_ICON_VAR, tm_tag_field_t | tm_tag_member_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t},
};

static TMParserMapEntry map_DOCBOOK[] = {
	{'f', tm_tag_function_t},
	{'c', tm_tag_class_t},
	{'m', tm_tag_member_t},
	{'d', tm_tag_macro_t},
	{'v', tm_tag_variable_t},
	{'s', tm_tag_struct_t},
};
static TMParserMapGroup group_DOCBOOK[] = {
	{_("Chapter"), TM_ICON_NONE, tm_tag_function_t},
	{_("Section"), TM_ICON_NONE, tm_tag_class_t},
	{_("Sect1"), TM_ICON_NONE, tm_tag_member_t},
	{_("Sect2"), TM_ICON_NONE, tm_tag_macro_t},
	{_("Sect3"), TM_ICON_NONE, tm_tag_variable_t},
	{_("Appendix"), TM_ICON_NONE, tm_tag_struct_t},
};

// no scope information
static TMParserMapEntry map_ERLANG[] = {
	{'d', tm_tag_macro_t},     // macro
	{'f', tm_tag_function_t},  // function
	{'m', tm_tag_undef_t},     // module
	{'r', tm_tag_struct_t},    // record
	{'t', tm_tag_typedef_t},   // type
};
static TMParserMapGroup group_ERLANG[] = {
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Structs"), TM_ICON_STRUCT, tm_tag_struct_t},
	{_("Typedefs / Enums"), TM_ICON_STRUCT, tm_tag_typedef_t},
	{_("Macros"), TM_ICON_MACRO, tm_tag_macro_t},
};

// no scope information
static TMParserMapEntry map_CSS[] = {
	{'c', tm_tag_class_t},     // class
	{'s', tm_tag_struct_t},    // selector
	{'i', tm_tag_variable_t},  // id
};
static TMParserMapGroup group_CSS[] = {
	{_("Classes"), TM_ICON_CLASS, tm_tag_class_t},
	{_("ID Selectors"), TM_ICON_VAR, tm_tag_variable_t},
	{_("Type Selectors"), TM_ICON_STRUCT, tm_tag_struct_t},
};

static TMParserMapEntry map_RUBY[] = {
	{'c', tm_tag_class_t},      // class
	{'f', tm_tag_method_t},     // method
	{'m', tm_tag_namespace_t},  // module
	{'S', tm_tag_member_t},     // singletonMethod
	{'C', tm_tag_undef_t},      // constant
	{'A', tm_tag_undef_t},      // accessor
	{'a', tm_tag_undef_t},      // alias
	{'L', tm_tag_undef_t},      // library
};
static TMParserMapGroup group_RUBY[] = {
	{_("Modules"), TM_ICON_NAMESPACE, tm_tag_namespace_t},
	{_("Classes"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Singletons"), TM_ICON_STRUCT, tm_tag_member_t},
	{_("Methods"), TM_ICON_METHOD, tm_tag_method_t},
};

static TMParserMapEntry map_TCL[] = {
	{'p', tm_tag_function_t},   // procedure
	{'n', tm_tag_namespace_t},  // namespace
	{'z', tm_tag_undef_t},      // parameter
};
static TMParserMapGroup group_TCL[] = {
	{_("Namespaces"), TM_ICON_NAMESPACE, tm_tag_namespace_t},
	{_("Classes"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Methods"), TM_ICON_METHOD, tm_tag_member_t},
	{_("Procedures"), TM_ICON_OTHER, tm_tag_function_t},
};

static TMParserMapEntry map_TCLOO[] = {
	{'c', tm_tag_class_t},   // class
	{'m', tm_tag_member_t},  // method
};
#define group_TCLOO group_TCL

static TMSubparserMapEntry subparser_TCLOO_TCL_map[] = {
	{tm_tag_namespace_t, tm_tag_namespace_t},
	{tm_tag_class_t, tm_tag_class_t},
	{tm_tag_member_t, tm_tag_member_t},
	{tm_tag_function_t, tm_tag_function_t},
};

static TMParserMapEntry map_SH[] = {
	{'a', tm_tag_undef_t},     // alias
	{'f', tm_tag_function_t},  // function
	{'s', tm_tag_undef_t},     // script
	{'h', tm_tag_undef_t},     // heredoc
};
static TMParserMapGroup group_SH[] = {
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
};

static TMParserMapEntry map_D[] = {
	{'c', tm_tag_class_t},       // class
	{'e', tm_tag_enumerator_t},  // enumerator
	{'f', tm_tag_function_t},    // function
	{'g', tm_tag_enum_t},        // enum
	{'i', tm_tag_interface_t},   // interface
	{'m', tm_tag_member_t},      // member
	{'n', tm_tag_namespace_t},   // namespace
	{'p', tm_tag_prototype_t},   // prototype
	{'s', tm_tag_struct_t},      // struct
	{'t', tm_tag_typedef_t},     // typedef
	{'u', tm_tag_union_t},       // union
	{'v', tm_tag_variable_t},    // variable
	{'x', tm_tag_externvar_t},   // externvar
};
static TMParserMapGroup group_D[] = {
	{_("Module"), TM_ICON_NONE, tm_tag_namespace_t},
	{_("Classes"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Interfaces"), TM_ICON_STRUCT, tm_tag_interface_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t | tm_tag_prototype_t},
	{_("Members"), TM_ICON_MEMBER, tm_tag_member_t},
	{_("Structs"), TM_ICON_STRUCT, tm_tag_struct_t | tm_tag_union_t},
	{_("Typedefs / Enums"), TM_ICON_STRUCT, tm_tag_typedef_t | tm_tag_enum_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t | tm_tag_enumerator_t},
	{_("Extern Variables"), TM_ICON_VAR, tm_tag_externvar_t},
};

static TMParserMapEntry map_DIFF[] = {
	{'m', tm_tag_function_t},  // modifiedFile
	{'n', tm_tag_function_t},  // newFile
	{'d', tm_tag_function_t},  // deletedFile
	{'h', tm_tag_undef_t},     // hunk
};
static TMParserMapGroup group_DIFF[] = {
	{_("Files"), TM_ICON_NONE, tm_tag_function_t},
};

static TMParserMapEntry map_VHDL[] = {
	{'c', tm_tag_variable_t},   // constant
	{'t', tm_tag_typedef_t},    // type
	{'T', tm_tag_typedef_t},    // subtype
	{'r', tm_tag_undef_t},      // record
	{'e', tm_tag_class_t},      // entity
	{'C', tm_tag_member_t},     // component
	{'d', tm_tag_undef_t},      // prototype
	{'f', tm_tag_function_t},   // function
	{'p', tm_tag_function_t},   // procedure
	{'P', tm_tag_namespace_t},  // package
	{'l', tm_tag_variable_t},   // local
	{'a', tm_tag_struct_t},     // architecture
	{'q', tm_tag_variable_t},   // port
	{'g', tm_tag_undef_t},      // generic
	{'s', tm_tag_variable_t},   // signal
	{'Q', tm_tag_member_t},     // process
	{'v', tm_tag_variable_t},   // variable
	{'A', tm_tag_typedef_t},    // alias
};
static TMParserMapGroup group_VHDL[] = {
	{_("Package"), TM_ICON_NAMESPACE, tm_tag_namespace_t},
	{_("Entities"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Architectures"), TM_ICON_STRUCT, tm_tag_struct_t},
	{_("Types"), TM_ICON_OTHER, tm_tag_typedef_t},
	{_("Functions / Procedures"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Variables / Signals / Ports"), TM_ICON_VAR, tm_tag_variable_t},
	{_("Processes / Blocks / Components"), TM_ICON_MEMBER, tm_tag_member_t},
};

static TMParserMapEntry map_LUA[] = {
	{'f', tm_tag_function_t},  // function
	{'X', tm_tag_undef_t},     // unknown
};
static TMParserMapGroup group_LUA[] = {
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
};

static TMParserMapEntry map_JAVASCRIPT[] = {
	{'f', tm_tag_function_t},  // function
	{'c', tm_tag_class_t},     // class
	{'m', tm_tag_method_t},    // method
	{'p', tm_tag_member_t},    // property
	{'C', tm_tag_macro_t},     // constant
	{'v', tm_tag_variable_t},  // variable
	{'g', tm_tag_function_t},  // generator
	{'G', tm_tag_undef_t},     // getter
	{'S', tm_tag_undef_t},     // setter
	{'M', tm_tag_undef_t},     // field
};
static TMParserMapGroup group_JAVASCRIPT[] = {
	{_("Classes"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t | tm_tag_method_t},
	{_("Members"), TM_ICON_MEMBER, tm_tag_member_t},
	{_("Macros"), TM_ICON_MACRO, tm_tag_macro_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t},
};

// no scope information
static TMParserMapEntry map_HASKELL[] = {
	{'t', tm_tag_typedef_t},    // type
	{'c', tm_tag_macro_t},      // constructor
	{'f', tm_tag_function_t},   // function
	{'m', tm_tag_namespace_t},  // module
};
static TMParserMapGroup group_HASKELL[] = {
	{_("Module"), TM_ICON_NONE, tm_tag_namespace_t},
	{_("Types"), TM_ICON_NONE, tm_tag_typedef_t},
	{_("Type constructors"), TM_ICON_NONE, tm_tag_macro_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
};

#define map_UNUSED1 map_HASKELL
#define group_UNUSED1 group_HASKELL

static TMParserMapEntry map_CSHARP[] = {
	{'c', tm_tag_class_t},       // class
	{'d', tm_tag_macro_t},       // macro
	{'e', tm_tag_enumerator_t},  // enumerator
	{'E', tm_tag_undef_t},       // event
	{'f', tm_tag_field_t},       // field
	{'g', tm_tag_enum_t},        // enum
	{'i', tm_tag_interface_t},   // interface
	{'l', tm_tag_undef_t},       // local
	{'m', tm_tag_method_t},      // method
	{'n', tm_tag_namespace_t},   // namespace
	{'p', tm_tag_undef_t},       // property
	{'s', tm_tag_struct_t},      // struct
	{'t', tm_tag_typedef_t},     // typedef
};
#define group_CSHARP group_C

// no scope information
static TMParserMapEntry map_FREEBASIC[] = {
	{'c', tm_tag_macro_t},      // constant
	{'f', tm_tag_function_t},   // function
	{'l', tm_tag_namespace_t},  // label
	{'t', tm_tag_struct_t},     // type
	{'v', tm_tag_variable_t},   // variable
	{'g', tm_tag_externvar_t},  // enum
};
static TMParserMapGroup group_FREEBASIC[] = {
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t | tm_tag_externvar_t},
	{_("Constants"), TM_ICON_MACRO, tm_tag_macro_t},
	{_("Types"), TM_ICON_NAMESPACE, tm_tag_struct_t},
	{_("Labels"), TM_ICON_MEMBER, tm_tag_namespace_t},
};

// no scope information
static TMParserMapEntry map_HAXE[] = {
	{'m', tm_tag_method_t},     // method
	{'c', tm_tag_class_t},      // class
	{'e', tm_tag_enum_t},       // enum
	{'v', tm_tag_variable_t},   // variable
	{'i', tm_tag_interface_t},  // interface
	{'t', tm_tag_typedef_t},    // typedef
};
static TMParserMapGroup group_HAXE[] = {
	{_("Interfaces"), TM_ICON_STRUCT, tm_tag_interface_t},
	{_("Classes"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Methods"), TM_ICON_METHOD, tm_tag_method_t},
	{_("Types"), TM_ICON_MACRO, tm_tag_typedef_t | tm_tag_enum_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t},
};

static TMParserMapEntry map_REST[] = {
	{'c', tm_tag_namespace_t},  // chapter
	{'s', tm_tag_member_t},     // section
	{'S', tm_tag_macro_t},      // subsection
	{'t', tm_tag_variable_t},   // subsubsection
	{'C', tm_tag_undef_t},      // citation
	{'T', tm_tag_undef_t},      // target
	{'d', tm_tag_undef_t},      // substdef
};
static TMParserMapGroup group_REST[] = {
	{_("Chapter"), TM_ICON_NONE, tm_tag_namespace_t},
	{_("Section"), TM_ICON_NONE, tm_tag_member_t},
	{_("Subsection"), TM_ICON_NONE, tm_tag_macro_t},
	{_("Subsubsection"), TM_ICON_NONE, tm_tag_variable_t},
};

// no scope information
static TMParserMapEntry map_HTML[] = {
	{'a', tm_tag_member_t},     // anchor
	{'c', tm_tag_undef_t},      // class
	{'h', tm_tag_namespace_t},  // heading1
	{'i', tm_tag_class_t},      // heading2
	{'j', tm_tag_variable_t},   // heading3
	{'C', tm_tag_undef_t},      // stylesheet
	{'I', tm_tag_undef_t},      // id
	{'J', tm_tag_undef_t},      // script
};
static TMParserMapGroup group_HTML[] = {
	{_("Functions"), TM_ICON_NONE, tm_tag_function_t},  // javascript functions from subparser
	{_("Anchors"), TM_ICON_NONE, tm_tag_member_t},
	{_("H1 Headings"), TM_ICON_NONE, tm_tag_namespace_t},
	{_("H2 Headings"), TM_ICON_NONE, tm_tag_class_t},
	{_("H3 Headings"), TM_ICON_NONE, tm_tag_variable_t},
};

static TMSubparserMapEntry subparser_HTML_javascript_map[] = {
	{tm_tag_function_t, tm_tag_function_t},
};

static TMParserMapEntry map_FORTRAN[] = {
	{'b', tm_tag_undef_t},       // blockData
	{'c', tm_tag_macro_t},       // common
	{'e', tm_tag_undef_t},       // entry
	{'E', tm_tag_enum_t},        // enum
	{'f', tm_tag_function_t},    // function
	{'i', tm_tag_interface_t},   // interface
	{'k', tm_tag_member_t},      // component
	{'l', tm_tag_undef_t},       // label
	{'L', tm_tag_undef_t},       // local
	{'m', tm_tag_namespace_t},   // module
	{'M', tm_tag_member_t},      // method
	{'n', tm_tag_undef_t},       // namelist
	{'N', tm_tag_enumerator_t},  // enumerator
	{'p', tm_tag_struct_t},      // program
	{'P', tm_tag_undef_t},       // prototype
	{'s', tm_tag_method_t},      // subroutine
	{'t', tm_tag_class_t},       // type
	{'v', tm_tag_variable_t},    // variable
	{'S', tm_tag_undef_t},       // submodule
};
static TMParserMapGroup group_FORTRAN[] = {
	{_("Module"), TM_ICON_CLASS, tm_tag_namespace_t},
	{_("Programs"), TM_ICON_CLASS, tm_tag_struct_t},
	{_("Interfaces"), TM_ICON_STRUCT, tm_tag_interface_t},
	{_("Functions / Subroutines"), TM_ICON_METHOD, tm_tag_function_t | tm_tag_method_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t | tm_tag_enumerator_t},
	{_("Types"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Components"), TM_ICON_MEMBER, tm_tag_member_t},
	{_("Blocks"), TM_ICON_MEMBER, tm_tag_macro_t},
	{_("Enums"), TM_ICON_STRUCT, tm_tag_enum_t},
};

static TMParserMapEntry map_MATLAB[] = {
	{'f', tm_tag_function_t},  // function
	{'s', tm_tag_struct_t},    // struct
};
static TMParserMapGroup group_MATLAB[] = {
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Structures"), TM_ICON_STRUCT, tm_tag_struct_t},
};

#define map_CUDA map_C
#define group_CUDA group_C

static TMParserMapEntry map_VALA[] = {
	{'c', tm_tag_class_t},       // class
	{'d', tm_tag_macro_t},       // macro
	{'e', tm_tag_enumerator_t},  // enumerator
	{'f', tm_tag_field_t},       // field
	{'g', tm_tag_enum_t},        // enum
	{'i', tm_tag_interface_t},   // interface
	{'l', tm_tag_undef_t},       // local
	{'m', tm_tag_method_t},      // method
	{'n', tm_tag_namespace_t},   // namespace
	{'p', tm_tag_undef_t},       // property
	{'S', tm_tag_undef_t},       // signal
	{'s', tm_tag_struct_t},      // struct
};
#define group_VALA group_C

static TMParserMapEntry map_ACTIONSCRIPT[] = {
	{'f', tm_tag_function_t},   // function
	{'c', tm_tag_class_t},      // class
	{'i', tm_tag_interface_t},  // interface
	{'P', tm_tag_package_t},    // package
	{'m', tm_tag_method_t},     // method
	{'p', tm_tag_member_t},     // property
	{'v', tm_tag_variable_t},   // variable
	{'l', tm_tag_variable_t},   // localvar
	{'C', tm_tag_macro_t},      // constant
	{'I', tm_tag_externvar_t},  // import
	{'x', tm_tag_other_t},      // mxtag
};
static TMParserMapGroup group_ACTIONSCRIPT[] = {
	{_("Imports"), TM_ICON_NAMESPACE, tm_tag_externvar_t},
	{_("Package"), TM_ICON_NAMESPACE, tm_tag_package_t},
	{_("Interfaces"), TM_ICON_STRUCT, tm_tag_interface_t},
	{_("Classes"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t| tm_tag_method_t},
	{_("Properties"), TM_ICON_MEMBER, tm_tag_member_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t},
	{_("Constants"), TM_ICON_MACRO, tm_tag_macro_t},
	{_("Other"), TM_ICON_OTHER, tm_tag_other_t},
};

static TMParserMapEntry map_NSIS[] = {
	{'s', tm_tag_namespace_t},  // section
	{'f', tm_tag_function_t},   // function
	{'v', tm_tag_variable_t},   // variable
	{'d', tm_tag_undef_t},      // definition
	{'m', tm_tag_undef_t},      // macro
	{'S', tm_tag_undef_t},      // sectionGroup
	{'p', tm_tag_undef_t},      // macroparam
	{'l', tm_tag_undef_t},      // langstr
	{'i', tm_tag_undef_t},      // script
};
static TMParserMapGroup group_NSIS[] = {
	{_("Sections"), TM_ICON_OTHER, tm_tag_namespace_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t},
};

static TMParserMapEntry map_MARKDOWN[] = {
	{'c', tm_tag_namespace_t},  //chapter
	{'s', tm_tag_member_t},     //section
	{'S', tm_tag_macro_t},      //subsection
	{'t', tm_tag_variable_t},   //subsubsection
	{'T', tm_tag_struct_t},     //l4subsection
	{'u', tm_tag_union_t},      //l5subsection
	{'n', tm_tag_undef_t},      //footnote
};
static TMParserMapGroup group_MARKDOWN[] = {
	{_("Chapters"), TM_ICON_NONE, tm_tag_namespace_t},
	{_("Sections"), TM_ICON_NONE, tm_tag_member_t},
	{_("Subsections"), TM_ICON_NONE, tm_tag_macro_t},
	{_("Subsubsections"), TM_ICON_NONE, tm_tag_variable_t},
	{_("Level 4 sections"), TM_ICON_NONE, tm_tag_struct_t},
	{_("Level 5 sections"), TM_ICON_NONE, tm_tag_union_t},
};

static TMParserMapEntry map_TXT2TAGS[] = {
	{'s', tm_tag_member_t},  // section
};
#define group_TXT2TAGS group_REST

// no scope information
static TMParserMapEntry map_ABC[] = {
	{'s', tm_tag_member_t},  // section
};
#define group_ABC group_REST

static TMParserMapEntry map_VERILOG[] = {
	{'c', tm_tag_variable_t},  // constant
	{'e', tm_tag_typedef_t},   // event
	{'f', tm_tag_function_t},  // function
	{'m', tm_tag_class_t},     // module
	{'n', tm_tag_variable_t},  // net
	{'p', tm_tag_variable_t},  // port
	{'r', tm_tag_variable_t},  // register
	{'t', tm_tag_function_t},  // task
	{'b', tm_tag_undef_t},     // block
	{'i', tm_tag_undef_t},     // instance
};
static TMParserMapGroup group_VERILOG[] = {
	{_("Events"), TM_ICON_MACRO, tm_tag_typedef_t},
	{_("Modules"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Functions / Tasks"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t},
};

static TMParserMapEntry map_R[] = {
	{'f', tm_tag_function_t},  // function
	{'l', tm_tag_other_t},     // library
	{'s', tm_tag_other_t},     // source
	{'g', tm_tag_undef_t},     // globalVar
	{'v', tm_tag_undef_t},     // functionVar
	{'z', tm_tag_undef_t},     // parameter
	{'c', tm_tag_undef_t},     // vector
	{'L', tm_tag_undef_t},     // list
	{'d', tm_tag_undef_t},     // dataframe
	{'n', tm_tag_undef_t},     // nameattr
};
static TMParserMapGroup group_R[] = {
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Other"), TM_ICON_NONE, tm_tag_other_t},
};

static TMParserMapEntry map_COBOL[] = {
	{'f', tm_tag_function_t},   // fd
	{'g', tm_tag_struct_t},     // group
	{'P', tm_tag_class_t},      // program
	{'s', tm_tag_namespace_t},  // section
	{'D', tm_tag_interface_t},  // division
	{'p', tm_tag_macro_t},      // paragraph
	{'d', tm_tag_variable_t},   // data
	{'S', tm_tag_externvar_t},  // sourcefile
};
static TMParserMapGroup group_COBOL[] = {
	{_("Program"), TM_ICON_CLASS, tm_tag_class_t},
	{_("File"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Divisions"), TM_ICON_NAMESPACE, tm_tag_interface_t},
	{_("Sections"), TM_ICON_NAMESPACE, tm_tag_namespace_t},
	{_("Paragraph"), TM_ICON_OTHER, tm_tag_macro_t},
	{_("Group"), TM_ICON_STRUCT, tm_tag_struct_t},
	{_("Data"), TM_ICON_VAR, tm_tag_variable_t},
	{_("Copies"), TM_ICON_NAMESPACE, tm_tag_externvar_t},
};

static TMParserMapEntry map_OBJC[] = {
	{'i', tm_tag_interface_t},  // interface
	{'I', tm_tag_undef_t},      // implementation
	{'P', tm_tag_undef_t},      // protocol
	{'m', tm_tag_method_t},     // method
	{'c', tm_tag_class_t},      // class
	{'v', tm_tag_variable_t},   // var
	{'E', tm_tag_field_t},      // field
	{'f', tm_tag_function_t},   // function
	{'p', tm_tag_undef_t},      // property
	{'t', tm_tag_typedef_t},    // typedef
	{'s', tm_tag_struct_t},     // struct
	{'e', tm_tag_enum_t},       // enum
	{'M', tm_tag_macro_t},      // macro
	{'C', tm_tag_undef_t},      // category
};
#define group_OBJC group_C

static TMParserMapEntry map_ASCIIDOC[] = {
	{'c', tm_tag_namespace_t},  //chapter
	{'s', tm_tag_member_t},     //section
	{'S', tm_tag_macro_t},      //subsection
	{'t', tm_tag_variable_t},   //subsubsection
	{'T', tm_tag_struct_t},     //l4subsection
	{'u', tm_tag_undef_t},      //l5subsection
	{'a', tm_tag_undef_t},      //anchor
};
static TMParserMapGroup group_ASCIIDOC[] = {
	{_("Document"), TM_ICON_NONE, tm_tag_namespace_t},
	{_("Section Level 1"), TM_ICON_NONE, tm_tag_member_t},
	{_("Section Level 2"), TM_ICON_NONE, tm_tag_macro_t},
	{_("Section Level 3"), TM_ICON_NONE, tm_tag_variable_t},
	{_("Section Level 4"), TM_ICON_NONE, tm_tag_struct_t},
};

// no scope information
static TMParserMapEntry map_ABAQUS[] = {
	{'p', tm_tag_class_t},      // part
	{'a', tm_tag_member_t},     // assembly
	{'s', tm_tag_interface_t},  // step
};
static TMParserMapGroup group_ABAQUS[] = {
	{_("Parts"), TM_ICON_NONE, tm_tag_class_t},
	{_("Assembly"), TM_ICON_NONE, tm_tag_member_t},
	{_("Steps"), TM_ICON_NONE, tm_tag_interface_t},
};

static TMParserMapEntry map_RUST[] = {
	{'n', tm_tag_namespace_t},   // module
	{'s', tm_tag_struct_t},      // struct
	{'i', tm_tag_interface_t},   // interface
	{'c', tm_tag_class_t},       // implementation
	{'f', tm_tag_function_t},    // function
	{'g', tm_tag_enum_t},        // enum
	{'t', tm_tag_typedef_t},     // typedef
	{'v', tm_tag_variable_t},    // variable
	{'M', tm_tag_macro_t},       // macro
	{'m', tm_tag_field_t},       // field
	{'e', tm_tag_enumerator_t},  // enumerator
	{'P', tm_tag_method_t},      // method
};
static TMParserMapGroup group_RUST[] = {
	{_("Modules"), TM_ICON_NAMESPACE, tm_tag_namespace_t},
	{_("Structures"), TM_ICON_STRUCT, tm_tag_struct_t},
	{_("Traits"), TM_ICON_CLASS, tm_tag_interface_t},
	{_("Implementations"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t | tm_tag_method_t},
	{_("Typedefs / Enums"), TM_ICON_STRUCT, tm_tag_typedef_t | tm_tag_enum_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t | tm_tag_enumerator_t},
	{_("Macros"), TM_ICON_MACRO, tm_tag_macro_t},
	{_("Methods"), TM_ICON_MEMBER, tm_tag_field_t},
};

static TMParserMapEntry map_GO[] = {
	{'p', tm_tag_namespace_t},  // package
	{'f', tm_tag_function_t},   // func
	{'c', tm_tag_macro_t},      // const
	{'t', tm_tag_typedef_t},    // type
	{'v', tm_tag_variable_t},   // var
	{'s', tm_tag_struct_t},     // struct
	{'i', tm_tag_interface_t},  // interface
	{'m', tm_tag_member_t},     // member
	{'M', tm_tag_undef_t},      // anonMember
	{'n', tm_tag_undef_t},      // methodSpec
	{'u', tm_tag_undef_t},      // unknown
	{'P', tm_tag_undef_t},      // packageName
	{'a', tm_tag_undef_t},      // talias
	{'R', tm_tag_undef_t},      // receiver
};
static TMParserMapGroup group_GO[] = {
	{_("Package"), TM_ICON_NAMESPACE, tm_tag_namespace_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Interfaces"), TM_ICON_STRUCT, tm_tag_interface_t},
	{_("Structs"), TM_ICON_STRUCT, tm_tag_struct_t},
	{_("Types"), TM_ICON_STRUCT, tm_tag_typedef_t},
	{_("Constants"), TM_ICON_MACRO, tm_tag_macro_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t},
	{_("Members"), TM_ICON_MEMBER, tm_tag_member_t},
};

static TMParserMapEntry map_JSON[] = {
	{'o', tm_tag_member_t},  // object
	{'a', tm_tag_member_t},  // array
	{'n', tm_tag_member_t},  // number
	{'s', tm_tag_member_t},  // string
	{'b', tm_tag_member_t},  // boolean
	{'z', tm_tag_member_t},  // null
};
static TMParserMapGroup group_JSON[] = {
	{_("Members"), TM_ICON_MEMBER, tm_tag_member_t},
};

/* Zephir, same as PHP */
#define map_ZEPHIR map_PHP
#define group_ZEPHIR group_PHP

static TMParserMapEntry map_POWERSHELL[] = {
	{'f', tm_tag_function_t},  // function
	{'v', tm_tag_variable_t},  // variable
};
static TMParserMapGroup group_POWERSHELL[] = {
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t},
};

static TMParserMapEntry map_JULIA[] = {
	{'c', tm_tag_variable_t},   // constant
	{'f', tm_tag_function_t},   // function
	{'g', tm_tag_member_t},     // field
	{'m', tm_tag_macro_t},      // macro
	{'n', tm_tag_namespace_t},  // module
	{'s', tm_tag_struct_t},     // struct
	{'t', tm_tag_typedef_t},    // type
    /* defined as externvar to get those excluded as forward type in symbols.c:goto_tag()
     * so we can jump to the real implementation (if known) instead of to the import statement */
	{'x', tm_tag_externvar_t},  // unknown
};
static TMParserMapGroup group_JULIA[] = {
	{_("Constants"), TM_ICON_VAR, tm_tag_variable_t},
	{_("Modules"), TM_ICON_NAMESPACE, tm_tag_namespace_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Fields"), TM_ICON_MEMBER, tm_tag_member_t},
	{_("Macros"), TM_ICON_MACRO, tm_tag_macro_t},
	{_("Structures"), TM_ICON_STRUCT, tm_tag_struct_t},
	{_("Types"), TM_ICON_CLASS, tm_tag_typedef_t},
	{_("Unknowns"), TM_ICON_OTHER, tm_tag_externvar_t},
};

static TMParserMapEntry map_CPREPROCESSOR[] = {
	{'d', tm_tag_undef_t},  // macro
	{'h', tm_tag_undef_t},  // header
	{'D', tm_tag_undef_t},  // parameter
};
#define group_CPREPROCESSOR group_C

static TMParserMapEntry map_GDSCRIPT[] = {
	{'c', tm_tag_class_t},     // class
	{'m', tm_tag_method_t},    // method
	{'v', tm_tag_variable_t},  // variable
	{'C', tm_tag_variable_t},  // const
	{'g', tm_tag_enum_t},      // enum
	{'e', tm_tag_variable_t},  // enumerator
	{'z', tm_tag_local_var_t}, // parameter
	{'l', tm_tag_local_var_t}, // local
	{'s', tm_tag_variable_t},  // signal
};
static TMParserMapGroup group_GDSCRIPT[] = {
	{_("Classes"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Methods"), TM_ICON_MACRO, tm_tag_method_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t | tm_tag_local_var_t},
	{_("Enums"), TM_ICON_STRUCT, tm_tag_enum_t},
};

static TMParserMapEntry map_CLOJURE[] = {
	{'f', tm_tag_function_t},   // function
	{'n', tm_tag_namespace_t},  // namespace
};
static TMParserMapGroup group_CLOJURE[] = {
	{_("Namespaces"), TM_ICON_NAMESPACE, tm_tag_namespace_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
};

static TMParserMapEntry map_LISP[] = {
	{'u', tm_tag_undef_t},     // unknown
	{'f', tm_tag_function_t},  // function
	{'v', tm_tag_variable_t},  // variable
	{'m', tm_tag_macro_t},     // macro
	{'c', tm_tag_field_t},     // const
};
static TMParserMapGroup group_LISP[] = {
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t},
	{_("Macros"), TM_ICON_MACRO, tm_tag_macro_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t},
	{_("Constants"), TM_ICON_VAR, tm_tag_field_t},
};

static TMParserMapEntry map_TYPESCRIPT[] = {
	{'f', tm_tag_function_t},    // function
	{'c', tm_tag_class_t},       // class
	{'i', tm_tag_interface_t},   // interface
	{'g', tm_tag_enum_t},        // enum
	{'e', tm_tag_enumerator_t},  // enumerator
	{'m', tm_tag_method_t},      // method
	{'n', tm_tag_namespace_t},   // namespace
	{'z', tm_tag_local_var_t},   // parameter
	{'p', tm_tag_member_t},      // property
	{'v', tm_tag_variable_t},    // variable
	{'l', tm_tag_local_var_t},   // local
	{'C', tm_tag_macro_t},       // constant
	{'G', tm_tag_undef_t},       // generator
	{'a', tm_tag_undef_t},       // alias
};
static TMParserMapGroup group_TYPESCRIPT[] = {
	{_("Namespaces"), TM_ICON_NAMESPACE, tm_tag_namespace_t},
	{_("Classes"), TM_ICON_CLASS, tm_tag_class_t},
	{_("Interfaces"), TM_ICON_STRUCT, tm_tag_interface_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t | tm_tag_method_t},
	{_("Enums"), TM_ICON_STRUCT, tm_tag_enum_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t | tm_tag_local_var_t},
	{_("Constants"), TM_ICON_MACRO, tm_tag_macro_t},
	{_("Other"), TM_ICON_MEMBER, tm_tag_member_t | tm_tag_enumerator_t},
};

static TMParserMapEntry map_ADA[] = {
	{'P', tm_tag_package_t},     // packspec
	{'p', tm_tag_package_t},     // package
	{'T', tm_tag_typedef_t},     // typespec
	{'t', tm_tag_typedef_t},     // type
	{'U', tm_tag_undef_t},       // subspec
	{'u', tm_tag_typedef_t},     // subtype
	{'c', tm_tag_member_t},      // component
	{'l', tm_tag_enumerator_t},  // literal
	{'V', tm_tag_undef_t},       // varspec
	{'v', tm_tag_variable_t},    // variable
	{'f', tm_tag_undef_t},       // formal
	{'n', tm_tag_macro_t},       // constant
	{'x', tm_tag_undef_t},       // exception
	{'R', tm_tag_prototype_t},   // subprogspec
	{'r', tm_tag_function_t},    // subprogram
	{'K', tm_tag_prototype_t},   // taskspec
	{'k', tm_tag_method_t},      // task
	{'O', tm_tag_undef_t},       // protectspec
	{'o', tm_tag_undef_t},       // protected
	{'E', tm_tag_undef_t},       // entryspec
	{'e', tm_tag_undef_t},       // entry
	{'b', tm_tag_undef_t},       // label
	{'i', tm_tag_undef_t},       // identifier
	{'a', tm_tag_undef_t},       // autovar
	{'y', tm_tag_undef_t},       // anon
};
static TMParserMapGroup group_ADA[] = {
	{_("Packages"), TM_ICON_NAMESPACE, tm_tag_package_t},
	{_("Types"), TM_ICON_STRUCT, tm_tag_typedef_t},
	{_("Functions"), TM_ICON_METHOD, tm_tag_function_t | tm_tag_prototype_t},
	{_("Tasks"), TM_ICON_METHOD, tm_tag_method_t},
	{_("Variables"), TM_ICON_VAR, tm_tag_variable_t},
	{_("Constants"), TM_ICON_MACRO, tm_tag_macro_t},
	{_("Other"), TM_ICON_MEMBER, tm_tag_member_t | tm_tag_enumerator_t},
};

typedef struct
{
    TMParserMapEntry *entries;
    guint size;
    TMParserMapGroup *groups;
    guint group_num;
} TMParserMap;

#define MAP_ENTRY(lang) [TM_PARSER_##lang] = {map_##lang, G_N_ELEMENTS(map_##lang), group_##lang, G_N_ELEMENTS(group_##lang)}

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
	MAP_ENTRY(BIBTEX),
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
	MAP_ENTRY(GDSCRIPT),
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
	MAP_ENTRY(ADA),
	MAP_ENTRY(CUDA),
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
	MAP_ENTRY(JULIA),
	MAP_ENTRY(CPREPROCESSOR),
	MAP_ENTRY(TCLOO),
	MAP_ENTRY(CLOJURE),
	MAP_ENTRY(LISP),
	MAP_ENTRY(TYPESCRIPT),
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


gint tm_parser_get_sidebar_group(TMParserType lang, TMTagType type)
{
	TMParserMap *map;
	guint i;

	if (lang >= TM_PARSER_COUNT)
		return -1;

	map = &parser_map[lang];
	for (i = 0; i < map->group_num; i++)
	{
		if (map->groups[i].types & type)
			return i + 1;  // "Symbols" group is always first
	}
	return -1;
}


const gchar *tm_parser_get_sidebar_info(TMParserType lang, gint group, guint *icon)
{
	const gchar *name;
	TMParserMap *map;
	TMParserMapGroup *grp;

	if (lang >= TM_PARSER_COUNT)
		return NULL;

	if (group == 0)
	{
		name = _("Symbols");
		*icon = TM_ICON_NAMESPACE;
	}
	else
	{
		map = &parser_map[lang];
		if (group > (gint)map->group_num)
			return NULL;

		grp = &map->groups[group - 1];
		name = grp->name;
		*icon = grp->icon;
	}
#ifdef GETTEXT_PACKAGE
	return g_dgettext(GETTEXT_PACKAGE, name);
#else
	return name;
#endif
}


static void add_subparser(TMParserType lang, TMParserType sublang, TMSubparserMapEntry *map, guint map_size)
{
	guint i;
	GPtrArray *mapping;
	GHashTable *lang_map = g_hash_table_lookup(subparser_map, GINT_TO_POINTER(lang));

	if (!lang_map)
	{
		lang_map = g_hash_table_new(g_direct_hash, g_direct_equal);
		g_hash_table_insert(subparser_map, GINT_TO_POINTER(lang), lang_map);
	}

	mapping = g_ptr_array_new();
	for (i = 0; i < map_size; i++)
		g_ptr_array_add(mapping, &map[i]);

	g_hash_table_insert(lang_map, GINT_TO_POINTER(sublang), mapping);
}


#define SUBPARSER_MAP_ENTRY(lang, sublang, map) add_subparser(TM_PARSER_##lang, TM_PARSER_##sublang, map, G_N_ELEMENTS(map))

static void init_subparser_map(void)
{
	SUBPARSER_MAP_ENTRY(HTML, JAVASCRIPT, subparser_HTML_javascript_map);
	SUBPARSER_MAP_ENTRY(TCLOO, TCL, subparser_TCLOO_TCL_map);
}


TMTagType tm_parser_get_subparser_type(TMParserType lang, TMParserType sublang, TMTagType type)
{
	guint i;
	GHashTable *lang_map;
	GPtrArray *mapping;

	if (!subparser_map)
	{
		subparser_map = g_hash_table_new(g_direct_hash, g_direct_equal);
		init_subparser_map();
	}

	lang_map = g_hash_table_lookup(subparser_map, GINT_TO_POINTER(lang));
	if (!lang_map)
		return tm_tag_undef_t;

	mapping = g_hash_table_lookup(lang_map, GINT_TO_POINTER(sublang));
	if (!mapping)
		return tm_tag_undef_t;

	for (i = 0; i < mapping->len; i++)
	{
		TMSubparserMapEntry *entry = mapping->pdata[i];
		if (entry->orig_type == type)
			return entry->new_type;
	}

	return tm_tag_undef_t;
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
		TMTagType lang_types = 0;
		TMTagType group_types = 0;
		guint i;

		if (! map->entries || map->size < 1)
			g_error("No tag types in TM for %s, is the language listed in parser_map?",
					tm_ctags_get_lang_name(lang));

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
			lang_types |= map->entries[i].type;
		}

		for (i = 0; i < sizeof(presence_map); i++)
		{
			if (presence_map[i] > 1)
				g_error("Duplicate tag type '%c' found for %s",
					(gchar)i, tm_ctags_get_lang_name(lang));
		}

		for (i = 0; i < map->group_num; i++)
			group_types |= map->groups[i].types;

		if ((group_types & lang_types) != lang_types)
			g_warning("Not all tag types mapped to symbol tree groups for %s",
				tm_ctags_get_lang_name(lang));
	}
}


/* When the suffix of 'str' is an operator that should trigger scope
 * autocompletion, this function should return the length of the operator,
 * zero otherwise. */
gint tm_parser_scope_autocomplete_suffix(TMParserType lang, const gchar *str)
{
	const gchar *sep = tm_parser_scope_separator(lang);

	if (g_str_has_suffix(str, sep))
		return strlen(sep);

	switch (lang)
	{
		case TM_PARSER_C:
		case TM_PARSER_CPP:
			if (g_str_has_suffix(str, "."))
				return 1;
			else if (g_str_has_suffix(str, "->"))
				return 2;
			else if (lang == TM_PARSER_CPP && g_str_has_suffix(str, "->*"))
				return 3;
		default:
			break;
	}
	return 0;
}


/* Get the name of constructor method. Arguments of this method will be used
 * for calltips when creating an object using the class name
 * (e.g. after the opening brace in 'c = MyClass()' in Python) */
const gchar *tm_parser_get_constructor_method(TMParserType lang)
{
	switch (lang)
	{
		case TM_PARSER_D:
			return "this";
		case TM_PARSER_PYTHON:
			return "__init__";
		default:
			return NULL;
	}
}


/* determine anonymous tags from tag names only when corresponding
 * ctags information is not available */
gboolean tm_parser_is_anon_name(TMParserType lang, gchar *name)
{
	guint i;
	char dummy;

	if (sscanf(name, "__anon%u%c", &i, &dummy) == 1)  /* uctags tags files */
		return TRUE;
	else if (lang == TM_PARSER_C || lang == TM_PARSER_CPP)  /* legacy Geany tags files */
		return sscanf(name, "anon_%*[a-z]_%u%c", &i, &dummy) == 1;
	else if (lang == TM_PARSER_FORTRAN)  /* legacy Geany tags files */
	{
		return sscanf(name, "Structure#%u%c", &i, &dummy) == 1 ||
			sscanf(name, "Interface#%u%c", &i, &dummy) == 1 ||
			sscanf(name, "Enum#%u%c", &i, &dummy) == 1;
	}
	return FALSE;
}


static gchar *replace_string_if_present(gchar *haystack, gchar *needle, gchar *subst)
{
	if (strstr(haystack, needle))
	{
		gchar **split = g_strsplit(haystack, needle, -1);
		gchar *ret = g_strjoinv(subst, split);
		g_strfreev(split);
		return ret;
	}
	return haystack;
}


/* return updated scope or original scope if no change needed */
gchar *tm_parser_update_scope(TMParserType lang, gchar *scope)
{
	switch (lang)
	{
		case TM_PARSER_PHP:
		case TM_PARSER_ZEPHIR:
			/* PHP parser uses two different scope separators but this would
			 * complicate things in Geany so make sure there's just one type */
			return replace_string_if_present(scope, "\\", "::");
		case TM_PARSER_TCL:
		case TM_PARSER_TCLOO:
			/* The TCL(OO) parser returns scope prefixed with :: which we don't
			 * want. */
			if (g_str_has_prefix(scope, "::"))
				return g_strdup(scope + 2);
			break;
	}
	return scope;
}


/* whether or not to enable ctags roles for the given language and kind */
gboolean tm_parser_enable_role(TMParserType lang, gchar kind)
{
	switch (lang)
	{
		case TM_PARSER_GDSCRIPT:
			return kind == 'c' ? FALSE : TRUE;
		case TM_PARSER_GO:
			/* 'p' is used both for package definition tags and imported package
			 * tags and we can't tell which is which just by kind. By disabling
			 * roles for this kind, we only get package definition tags. */
			return kind == 'p' ? FALSE : TRUE;
	}
	return TRUE;
}


/* whether or not to enable ctags kinds for the given language */
gboolean tm_parser_enable_kind(TMParserType lang, gchar kind)
{
	TMParserMap *map;
	guint i;

	if (lang >= TM_PARSER_COUNT)
		/* Fatal error but tm_parser_verify_type_mappings() will provide
		 * better message later */
		return FALSE;

	map = &parser_map[lang];
	for (i = 0; i < map->size; i++)
	{
		if (map->entries[i].kind == kind)
			return map->entries[i].type != tm_tag_undef_t;
	}
	return FALSE;
}


gchar *tm_parser_format_variable(TMParserType lang, const gchar *name, const gchar *type)
{
	if (!type)
		return NULL;

	switch (lang)
	{
		case TM_PARSER_GO:
			return g_strconcat(name, " ", type, NULL);
		case TM_PARSER_PASCAL:
		case TM_PARSER_PYTHON:
			return g_strconcat(name, ": ", type, NULL);
		default:
			return g_strconcat(type, " ", name, NULL);
	}
}


gchar *tm_parser_format_function(TMParserType lang, const gchar *fname, const gchar *args,
	const gchar *retval, const gchar *scope)
{
	GString *str;

	if (!args)  /* not a function */
		return NULL;

	str = g_string_new(NULL);

	if (scope)
	{
		g_string_append(str, scope);
		g_string_append(str, tm_parser_scope_separator_printable(lang));
	}
	g_string_append(str, fname);
	g_string_append_c(str, ' ');
	g_string_append(str, args);

	if (retval)
	{
		switch (lang)
		{
			case TM_PARSER_GDSCRIPT:
			case TM_PARSER_GO:
			case TM_PARSER_PASCAL:
			case TM_PARSER_PYTHON:
			{
				/* retval after function */
				const gchar *sep;
				switch (lang)
				{
					case TM_PARSER_PASCAL:
						sep = ": ";
						break;
					case TM_PARSER_GDSCRIPT:
					case TM_PARSER_PYTHON:
						sep = " -> ";
						break;
					default:
						sep = " ";
						break;
				}
				g_string_append(str, sep);
				g_string_append(str, retval);
				break;
			}
			default:
				/* retval before function */
				g_string_prepend_c(str, ' ');
				g_string_prepend(str, retval);
				break;
		}
	}

	return g_string_free(str, FALSE);
}


const gchar *tm_parser_scope_separator(TMParserType lang)
{
	switch (lang)
	{
		case TM_PARSER_C:	/* for C++ .h headers or C structs */
		case TM_PARSER_CPP:
		case TM_PARSER_CUDA:
		case TM_PARSER_PHP:
		case TM_PARSER_POWERSHELL:
		case TM_PARSER_RUST:
		case TM_PARSER_TCL:
		case TM_PARSER_TCLOO:
		case TM_PARSER_ZEPHIR:
			return "::";

		case TM_PARSER_LATEX:
		case TM_PARSER_MARKDOWN:
		case TM_PARSER_TXT2TAGS:
			return "\"\"";

		/* these parsers don't report nested scopes but default "." for scope separator
		 * might appear in the text so use something more improbable */
		case TM_PARSER_ASCIIDOC:
		case TM_PARSER_CONF:
		case TM_PARSER_REST:
			return "\x3";

		default:
			return ".";
	}
}


const gchar *tm_parser_scope_separator_printable(TMParserType lang)
{
	switch (lang)
	{
		case TM_PARSER_ASCIIDOC:
		case TM_PARSER_CONF:
		case TM_PARSER_LATEX:
		case TM_PARSER_MARKDOWN:
		case TM_PARSER_REST:
		case TM_PARSER_TXT2TAGS:
			return " > ";

		default:
			return tm_parser_scope_separator(lang);
	}
}


gboolean tm_parser_has_full_scope(TMParserType lang)
{
	switch (lang)
	{
		/* These parsers include full hierarchy in the tag scope, separated by tm_parser_scope_separator() */
		case TM_PARSER_ACTIONSCRIPT:
		case TM_PARSER_C:
		case TM_PARSER_CPP:
		case TM_PARSER_CUDA:
		case TM_PARSER_CSHARP:
		case TM_PARSER_COBOL:
		case TM_PARSER_D:
		case TM_PARSER_GDSCRIPT:
		case TM_PARSER_GO:
		case TM_PARSER_JAVA:
		case TM_PARSER_JAVASCRIPT:
		case TM_PARSER_JSON:
		case TM_PARSER_LATEX:
		case TM_PARSER_LUA:
		case TM_PARSER_MARKDOWN:
		case TM_PARSER_PHP:
		case TM_PARSER_POWERSHELL:
		case TM_PARSER_PYTHON:
		case TM_PARSER_R:
		case TM_PARSER_RUBY:
		case TM_PARSER_RUST:
		case TM_PARSER_SQL:
		case TM_PARSER_TCL:
		case TM_PARSER_TCLOO:
		case TM_PARSER_TXT2TAGS:
		case TM_PARSER_TYPESCRIPT:
		case TM_PARSER_VALA:
		case TM_PARSER_VHDL:
		case TM_PARSER_VERILOG:
		case TM_PARSER_ZEPHIR:
			return TRUE;

		/* These make use of the scope, but don't include nested hierarchy
		 * (either as a parser limitation or a language semantic) */
		case TM_PARSER_ADA:
		case TM_PARSER_ASCIIDOC:
		case TM_PARSER_CLOJURE:
		case TM_PARSER_CONF:
		case TM_PARSER_ERLANG:
		case TM_PARSER_FORTRAN:
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
