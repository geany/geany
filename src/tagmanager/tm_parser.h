/*
*
*   Copyright (c) 2014 The Geany contributors
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

#ifndef TM_PARSER_H
#define TM_PARSER_H

#include <glib.h>

G_BEGIN_DECLS

/**
 Types of tags. It is a bitmask so that multiple tag types can
 be used simultaneously by 'OR'-ing them bitwise.
 e.g. tm_tag_class_t | tm_tag_struct_t
*/
typedef enum
{
	tm_tag_undef_t = 0, /**< Unknown type */
	tm_tag_class_t = 1, /**< Class declaration */
	tm_tag_enum_t = 2, /**< Enum declaration */
	tm_tag_enumerator_t = 4, /**< Enumerator value */
	tm_tag_field_t = 8, /**< Field (Java only) */
	tm_tag_function_t = 16, /**< Function definition */
	tm_tag_interface_t = 32, /**< Interface (Java only) */
	tm_tag_member_t = 64, /**< Member variable of class/struct */
	tm_tag_method_t = 128, /**< Class method (Java only) */
	tm_tag_namespace_t = 256, /**< Namespace declaration */
	tm_tag_package_t = 512, /**< Package (Java only) */
	tm_tag_prototype_t = 1024, /**< Function prototype */
	tm_tag_struct_t = 2048, /**< Struct declaration */
	tm_tag_typedef_t = 4096, /**< Typedef */
	tm_tag_union_t = 8192, /**< Union */
	tm_tag_variable_t = 16384, /**< Variable */
	tm_tag_externvar_t = 32768, /**< Extern or forward declaration */
	tm_tag_macro_t = 65536, /**<  Macro (without arguments) */
	tm_tag_macro_with_arg_t = 131072, /**< Parameterized macro */
	tm_tag_local_var_t = 262144, /**< Local variable (inside function) */
	tm_tag_other_t = 524288, /**< Other (non C/C++/Java tag) */
	tm_tag_include_t = 1048576, /**< C/C++ included header file name */
	tm_tag_max_t = 2097151 /**< Maximum value of TMTagType */
} TMTagType;


/** @gironly
 * A integral type which can hold known parser type IDs
 **/
typedef gint TMParserType;


/* keep in sync with icon names in symbols.c */
/**
 * Identifiers of icons used by Geany in the symbol tree, in the autocompletion popup,
 * and in the goto popup.
 */
typedef enum
{
	TM_ICON_CLASS,
	TM_ICON_MACRO,
	TM_ICON_MEMBER,
	TM_ICON_METHOD,
	TM_ICON_NAMESPACE,
	TM_ICON_OTHER,
	TM_ICON_STRUCT,
	TM_ICON_VAR,
	TM_ICON_NONE,
	TM_N_ICONS = TM_ICON_NONE
} TMIcon;


#ifdef GEANY_PRIVATE

/* keep in sync with tm_parsers.h and parser_map in tm_parser.c */
enum
{
	TM_PARSER_NONE = -2, /* keep in sync with ctags LANG_IGNORE */
	TM_PARSER_C = 0,
	TM_PARSER_CPP,
	TM_PARSER_JAVA,
	TM_PARSER_MAKEFILE,
	TM_PARSER_PASCAL,
	TM_PARSER_PERL,
	TM_PARSER_PHP,
	TM_PARSER_PYTHON,
	TM_PARSER_LATEX,
	TM_PARSER_ASM,
	TM_PARSER_CONF,
	TM_PARSER_SQL,
	TM_PARSER_DOCBOOK,
	TM_PARSER_ERLANG,
	TM_PARSER_CSS,
	TM_PARSER_RUBY,
	TM_PARSER_TCL,
	TM_PARSER_SH,
	TM_PARSER_D,
	TM_PARSER_FORTRAN,
	TM_PARSER_GDSCRIPT,
	TM_PARSER_DIFF,
	TM_PARSER_VHDL,
	TM_PARSER_LUA,
	TM_PARSER_JAVASCRIPT,
	TM_PARSER_HASKELL,
	TM_PARSER_CSHARP,
	TM_PARSER_FREEBASIC,
	TM_PARSER_HAXE,
	TM_PARSER_REST,
	TM_PARSER_HTML,
	TM_PARSER_ADA,
	TM_PARSER_CUDA,
	TM_PARSER_MATLAB,
	TM_PARSER_VALA,
	TM_PARSER_ACTIONSCRIPT,
	TM_PARSER_NSIS,
	TM_PARSER_MARKDOWN,
	TM_PARSER_TXT2TAGS,
	TM_PARSER_ABC,
	TM_PARSER_VERILOG,
	TM_PARSER_R,
	TM_PARSER_COBOL,
	TM_PARSER_OBJC,
	TM_PARSER_ASCIIDOC,
	TM_PARSER_ABAQUS,
	TM_PARSER_RUST,
	TM_PARSER_GO,
	TM_PARSER_JSON,
	TM_PARSER_ZEPHIR,
	TM_PARSER_POWERSHELL,
	TM_PARSER_JULIA,
	TM_PARSER_BIBTEX,
	TM_PARSER_CPREPROCESSOR,
	TM_PARSER_TCLOO,
	TM_PARSER_CLOJURE,
	TM_PARSER_LISP,
	TM_PARSER_TYPESCRIPT,
	TM_PARSER_BATCH,
	TM_PARSER_AUTOIT,
	TM_PARSER_RAKU,
	TM_PARSER_OCAML,
	TM_PARSER_LDSCRIPT,
	TM_PARSER_FORTH,
	TM_PARSER_MESON,
	TM_PARSER_SYSVERILOG,
	TM_PARSER_COUNT
};


void tm_parser_verify_type_mappings(void);

TMTagType tm_parser_get_tag_type(gchar kind, TMParserType lang);

gchar tm_parser_get_tag_kind(TMTagType type, TMParserType lang);

gint tm_parser_get_sidebar_group(TMParserType lang, TMTagType type);

const gchar *tm_parser_get_sidebar_info(TMParserType lang, gint group, guint *icon);

TMTagType tm_parser_get_subparser_type(TMParserType lang, TMParserType sublang, TMTagType type);

gint tm_parser_scope_autocomplete_suffix(TMParserType lang, const gchar *str);

const gchar *tm_parser_get_constructor_method(TMParserType lang);

gboolean tm_parser_is_anon_name(TMParserType lang, const gchar *name);

gchar *tm_parser_update_scope(TMParserType lang, gchar *scope);

gboolean tm_parser_enable_role(TMParserType lang, gchar kind);

gboolean tm_parser_enable_kind(TMParserType lang, gchar kind);

gchar *tm_parser_format_variable(TMParserType lang, const gchar *name, const gchar *type,
	const gchar *scope);

gchar *tm_parser_format_function(TMParserType lang, const gchar *fname, const gchar *args,
	const gchar *retval, const gchar *scope);

const gchar *tm_parser_scope_separator(TMParserType lang);

const gchar *tm_parser_scope_separator_printable(TMParserType lang);

gboolean tm_parser_has_full_scope(TMParserType lang);

gboolean tm_parser_langs_compatible(TMParserType lang, TMParserType other);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* TM_PARSER_H */
