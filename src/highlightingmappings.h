/*
 *      highlightingmappings.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005 The Geany contributors
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


#ifndef GEANY_HIGHLIGHTING_MAPPINGS_H
#define GEANY_HIGHLIGHTING_MAPPINGS_H 1

#include "Scintilla.h"
#include "SciLexer.h"

#include <glib.h>


G_BEGIN_DECLS

/* contains all filtypes informations in the form of:
 *  - highlighting_lexer_LANG:		the SCI lexer
 *  - highlighting_styles_LANG:		SCI style/named style mappings.  The first
 *									item is also used for the default style.
 *  - highlighting_keywords_LANG:	keywords ID/name mappings
 *  - highlighting_properties_LANG:	default SCI properties and their value
 * where LANG is the lang part from GEANY_FILETYPE_LANG
 *
 * Using this scheme makes possible to automate style setup by simply listing LANG
 * and let [init_]styleset_case() macros (in highlighting.c) prepare the correct
 * calls.
 */


typedef struct
{
	const gchar	*name;		/* style name in the filetypes.* file */
	guint		 style;		/* SCI style */
	gboolean	 fill_eol;	/* whether to set EOLFILLED flag to this style */
} HLStyle;

typedef struct
{
	const gchar	*key;	/* keywords entry name in the filetypes.* file */
	guint		 id;	/* SCI keyword ID */
	gboolean	 merge;	/* whether to merge with session types */
} HLKeyword;

typedef struct
{
	const gchar *property;
	const gchar *value;
} HLProperty;


#define EMPTY_KEYWORDS		((HLKeyword *) NULL)
#define EMPTY_PROPERTIES	((HLProperty *) NULL)

/* like G_N_ELEMENTS() but supports @array being NULL (for empty entries) */
#define HL_N_ENTRIES(array) ((array != NULL) ? G_N_ELEMENTS(array) : 0)


/* Abaqus */
#define highlighting_lexer_ABAQUS			SCLEX_ABAQUS
static const HLStyle highlighting_styles_ABAQUS[] =
{
	{ "default",			SCE_ABAQUS_DEFAULT,			FALSE },
	{ "comment",			SCE_ABAQUS_COMMENT,			FALSE },
	{ "number",				SCE_ABAQUS_NUMBER,			FALSE },
	{ "string",				SCE_ABAQUS_STRING,			FALSE },
	{ "operator",			SCE_ABAQUS_OPERATOR,		FALSE },
	{ "processor",			SCE_ABAQUS_PROCESSOR,		FALSE },
	{ "starcommand",		SCE_ABAQUS_STARCOMMAND,		FALSE },
	{ "argument",			SCE_ABAQUS_ARGUMENT,		FALSE }
};
static const HLKeyword highlighting_keywords_ABAQUS[] =
{
	{ "processors",			0,		FALSE },
	{ "commands",			1,		FALSE },
	{ "slashommands",		2,		FALSE },
	{ "starcommands",		3,		FALSE },
	{ "arguments",			4,		FALSE },
	{ "functions",			5,		FALSE }
};
#define highlighting_properties_ABAQUS		EMPTY_PROPERTIES


/* Ada */
#define highlighting_lexer_ADA			SCLEX_ADA
static const HLStyle highlighting_styles_ADA[] =
{
	{ "default",		SCE_ADA_DEFAULT,			FALSE },
	{ "word",			SCE_ADA_WORD,				FALSE },
	{ "identifier",		SCE_ADA_IDENTIFIER,			FALSE },
	{ "number",			SCE_ADA_NUMBER,				FALSE },
	{ "delimiter",		SCE_ADA_DELIMITER,			FALSE },
	{ "character",		SCE_ADA_CHARACTER,			FALSE },
	{ "charactereol",	SCE_ADA_CHARACTEREOL,		FALSE },
	{ "string",			SCE_ADA_STRING,				FALSE },
	{ "stringeol",		SCE_ADA_STRINGEOL,			FALSE },
	{ "label",			SCE_ADA_LABEL,				FALSE },
	{ "commentline",	SCE_ADA_COMMENTLINE,		FALSE },
	{ "illegal",		SCE_ADA_ILLEGAL,			FALSE }
};
static const HLKeyword highlighting_keywords_ADA[] =
{
	{ "primary", 0,	FALSE }
};
#define highlighting_properties_ADA		EMPTY_PROPERTIES


/* ActionScript */
#define highlighting_lexer_AS		SCLEX_CPP
#define highlighting_styles_AS		highlighting_styles_C
static const HLKeyword highlighting_keywords_AS[] =
{
	{ "primary",		0,	FALSE },
	{ "secondary",		1,	FALSE },
	{ "classes",		3,	FALSE }
};
#define highlighting_properties_AS	highlighting_properties_C


/* ASM */
#define highlighting_lexer_ASM			SCLEX_ASM
static const HLStyle highlighting_styles_ASM[] =
{
	{ "default",			SCE_ASM_DEFAULT,			FALSE },
	{ "comment",			SCE_ASM_COMMENT,			FALSE },
	{ "number",				SCE_ASM_NUMBER,				FALSE },
	{ "string",				SCE_ASM_STRING,				FALSE },
	{ "operator",			SCE_ASM_OPERATOR,			FALSE },
	{ "identifier",			SCE_ASM_IDENTIFIER,			FALSE },
	{ "cpuinstruction",		SCE_ASM_CPUINSTRUCTION,		FALSE },
	{ "mathinstruction",	SCE_ASM_MATHINSTRUCTION,	FALSE },
	{ "register",			SCE_ASM_REGISTER,			FALSE },
	{ "directive",			SCE_ASM_DIRECTIVE,			FALSE },
	{ "directiveoperand",	SCE_ASM_DIRECTIVEOPERAND,	FALSE },
	{ "commentblock",		SCE_ASM_COMMENTBLOCK,		FALSE },
	{ "character",			SCE_ASM_CHARACTER,			FALSE },
	{ "stringeol",			SCE_ASM_STRINGEOL,			FALSE },
	{ "extinstruction",		SCE_ASM_EXTINSTRUCTION,		FALSE },
	{ "commentdirective",	SCE_ASM_COMMENTDIRECTIVE,	FALSE }
};
static const HLKeyword highlighting_keywords_ASM[] =
{
	{ "instructions",	0,	FALSE },
	/*{ "instructions",	1,	FALSE },*/
	{ "registers",		2,	FALSE },
	{ "directives",		3,	FALSE }
	/*{ "instructions",	5,	FALSE }*/
};
#define highlighting_properties_ASM		EMPTY_PROPERTIES


/* BASIC */
#define highlighting_lexer_BASIC		SCLEX_FREEBASIC
static const HLStyle highlighting_styles_BASIC[] =
{
	{ "default",		SCE_B_DEFAULT,			FALSE },
	{ "comment",		SCE_B_COMMENT,			FALSE },
	{ "commentblock",	SCE_B_COMMENTBLOCK,		FALSE },
	{ "docline",		SCE_B_DOCLINE,			FALSE },
	{ "docblock",		SCE_B_DOCBLOCK,			FALSE },
	{ "dockeyword",		SCE_B_DOCKEYWORD,		FALSE },
	{ "number",			SCE_B_NUMBER,			FALSE },
	{ "word",			SCE_B_KEYWORD,			FALSE },
	{ "string",			SCE_B_STRING,			FALSE },
	{ "preprocessor",	SCE_B_PREPROCESSOR,		FALSE },
	{ "operator",		SCE_B_OPERATOR,			FALSE },
	{ "identifier",		SCE_B_IDENTIFIER,		FALSE },
	{ "date",			SCE_B_DATE,				FALSE },
	{ "stringeol",		SCE_B_STRINGEOL,		FALSE },
	{ "word2",			SCE_B_KEYWORD2,			FALSE },
	{ "word3",			SCE_B_KEYWORD3,			FALSE },
	{ "word4",			SCE_B_KEYWORD4,			FALSE },
	{ "constant",		SCE_B_CONSTANT,			FALSE },
	{ "asm",			SCE_B_ASM,				FALSE },
	{ "label",			SCE_B_LABEL,			FALSE },
	{ "error",			SCE_B_ERROR,			FALSE },
	{ "hexnumber",		SCE_B_HEXNUMBER,		FALSE },
	{ "binnumber",		SCE_B_BINNUMBER,		FALSE }
};
static const HLKeyword highlighting_keywords_BASIC[] =
{
	{ "keywords",		0,	FALSE },
	{ "preprocessor",	1,	FALSE },
	{ "user1",			2,	FALSE },
	{ "user2",			3,	FALSE }
};
#define highlighting_properties_BASIC	EMPTY_PROPERTIES


/* BATCH */
#define highlighting_lexer_BATCH		SCLEX_BATCH
static const HLStyle highlighting_styles_BATCH[] =
{
	{ "default",		SCE_BAT_DEFAULT,		FALSE },
	{ "comment",		SCE_BAT_COMMENT,		FALSE },
	{ "label",			SCE_BAT_LABEL,			FALSE },
	{ "word",			SCE_BAT_WORD,			FALSE },
	{ "hide",			SCE_BAT_HIDE,			FALSE },
	{ "command",		SCE_BAT_COMMAND,		FALSE },
	{ "identifier",		SCE_BAT_IDENTIFIER,		FALSE },
	{ "operator",		SCE_BAT_OPERATOR,		FALSE }
};
static const HLKeyword highlighting_keywords_BATCH[] =
{
	{ "keywords",			0,	FALSE },
	{ "keywords_optional",	1,	FALSE }
};
#define highlighting_properties_BATCH	EMPTY_PROPERTIES


/* C */
/* Also used by some other SCLEX_CPP-based filetypes */
#define highlighting_lexer_C		SCLEX_CPP
static const HLStyle highlighting_styles_C[] =
{
	{ "default",				SCE_C_DEFAULT,					FALSE },
	{ "comment",				SCE_C_COMMENT,					FALSE },
	{ "commentline",			SCE_C_COMMENTLINE,				FALSE },
	{ "commentdoc",				SCE_C_COMMENTDOC,				FALSE },
	{ "preprocessorcomment",	SCE_C_PREPROCESSORCOMMENT,		FALSE },
	{ "preprocessorcommentdoc",	SCE_C_PREPROCESSORCOMMENTDOC,	FALSE },
	{ "number",					SCE_C_NUMBER,					FALSE },
	{ "word",					SCE_C_WORD,						FALSE },
	{ "word2",					SCE_C_WORD2,					FALSE },
	{ "string",					SCE_C_STRING,					FALSE },
	{ "stringraw",				SCE_C_STRINGRAW,				FALSE },
	{ "character",				SCE_C_CHARACTER,				FALSE },
	{ "userliteral",			SCE_C_USERLITERAL,				FALSE },
	{ "uuid",					SCE_C_UUID,						FALSE },
	{ "preprocessor",			SCE_C_PREPROCESSOR,				FALSE },
	{ "operator",				SCE_C_OPERATOR,					FALSE },
	{ "identifier",				SCE_C_IDENTIFIER,				FALSE },
	{ "stringeol",				SCE_C_STRINGEOL,				FALSE },
	{ "verbatim",				SCE_C_VERBATIM,					FALSE },
	/* triple verbatims use the same style */
	{ "verbatim",				SCE_C_TRIPLEVERBATIM,			FALSE },
	{ "regex",					SCE_C_REGEX,					FALSE },
	{ "hashquotedstring",		SCE_C_HASHQUOTEDSTRING,			FALSE },
	{ "commentlinedoc",			SCE_C_COMMENTLINEDOC,			FALSE },
	{ "commentdockeyword",		SCE_C_COMMENTDOCKEYWORD,		FALSE },
	{ "commentdockeyworderror",	SCE_C_COMMENTDOCKEYWORDERROR,	FALSE },
	/* used for local structs and typedefs */
	{ "globalclass",			SCE_C_GLOBALCLASS,				FALSE },
	{ "taskmarker",				SCE_C_TASKMARKER,				FALSE },
	{ "escapesequence",			SCE_C_ESCAPESEQUENCE,			FALSE }
};
static const HLKeyword highlighting_keywords_C[] =
{
	{ "primary",	0,	FALSE },
	/* SCI_SETKEYWORDS = 1 - secondary + global tags file types, see below */
	{ "secondary",	1,	TRUE },
	{ "docComment", 2,	FALSE }
	/* SCI_SETKEYWORDS = 3 is for current session types - see document_highlight_tags() */
};
static const HLProperty highlighting_properties_C[] =
{
	{ "fold.cpp.comment.explicit", "0" }
};


/* Caml */
#define highlighting_lexer_CAML			SCLEX_CAML
static const HLStyle highlighting_styles_CAML[] =
{
	{ "default",		SCE_CAML_DEFAULT,			FALSE },
	{ "comment",		SCE_CAML_COMMENT,			FALSE },
	{ "comment1",		SCE_CAML_COMMENT1,			FALSE },
	{ "comment2",		SCE_CAML_COMMENT2,			FALSE },
	{ "comment3",		SCE_CAML_COMMENT3,			FALSE },
	{ "number",			SCE_CAML_NUMBER,			FALSE },
	{ "keyword",		SCE_CAML_KEYWORD,			FALSE },
	{ "keyword2",		SCE_CAML_KEYWORD2,			FALSE },
	{ "keyword3",		SCE_CAML_KEYWORD3,			FALSE },
	{ "string",			SCE_CAML_STRING,			FALSE },
	{ "char",			SCE_CAML_CHAR,				FALSE },
	{ "operator",		SCE_CAML_OPERATOR,			FALSE },
	{ "identifier",		SCE_CAML_IDENTIFIER,		FALSE },
	{ "tagname",		SCE_CAML_TAGNAME,			FALSE },
	{ "linenum",		SCE_CAML_LINENUM,			FALSE },
	{ "white",			SCE_CAML_WHITE,				FALSE }
};
static const HLKeyword highlighting_keywords_CAML[] =
{
	{ "keywords",			0,	FALSE },
	{ "keywords_optional",	1,	FALSE }
};
#define highlighting_properties_CAML	EMPTY_PROPERTIES


/* CMake */
#define highlighting_lexer_CMAKE		SCLEX_CMAKE
static const HLStyle highlighting_styles_CMAKE[] =
{
	{ "default",		SCE_CMAKE_DEFAULT,			FALSE },
	{ "comment",		SCE_CMAKE_COMMENT,			FALSE },
	{ "stringdq",		SCE_CMAKE_STRINGDQ,			FALSE },
	{ "stringlq",		SCE_CMAKE_STRINGLQ,			FALSE },
	{ "stringrq",		SCE_CMAKE_STRINGRQ,			FALSE },
	{ "command",		SCE_CMAKE_COMMANDS,			FALSE },
	{ "parameters",		SCE_CMAKE_PARAMETERS,		FALSE },
	{ "variable",		SCE_CMAKE_VARIABLE,			FALSE },
	{ "userdefined",	SCE_CMAKE_USERDEFINED,		FALSE },
	{ "whiledef",		SCE_CMAKE_WHILEDEF,			FALSE },
	{ "foreachdef",		SCE_CMAKE_FOREACHDEF,		FALSE },
	{ "ifdefinedef",	SCE_CMAKE_IFDEFINEDEF,		FALSE },
	{ "macrodef",		SCE_CMAKE_MACRODEF,			FALSE },
	{ "stringvar",		SCE_CMAKE_STRINGVAR,		FALSE },
	{ "number",			SCE_CMAKE_NUMBER,			FALSE }
};
static const HLKeyword highlighting_keywords_CMAKE[] =
{
	{ "commands",		0,	FALSE },
	{ "parameters",		1,	FALSE },
	{ "userdefined",	2,	FALSE }
};
#define highlighting_properties_CMAKE	EMPTY_PROPERTIES

/* CoffeeScript */
#define highlighting_lexer_COFFEESCRIPT		SCLEX_COFFEESCRIPT
static const HLStyle highlighting_styles_COFFEESCRIPT[] =
{
	{ "default",				SCE_COFFEESCRIPT_DEFAULT,				FALSE },
	{ "commentline",			SCE_COFFEESCRIPT_COMMENTLINE,			FALSE },
	{ "number",					SCE_COFFEESCRIPT_NUMBER,				FALSE },
	{ "word",					SCE_COFFEESCRIPT_WORD,					FALSE },
	{ "string",					SCE_COFFEESCRIPT_STRING,				FALSE },
	{ "character",				SCE_COFFEESCRIPT_CHARACTER,				FALSE },
	{ "operator",				SCE_COFFEESCRIPT_OPERATOR,				FALSE },
	{ "identifier",				SCE_COFFEESCRIPT_IDENTIFIER,			FALSE },
	{ "stringeol",				SCE_COFFEESCRIPT_STRINGEOL,				FALSE },
	{ "regex",					SCE_COFFEESCRIPT_REGEX,					FALSE },
	{ "word2",					SCE_COFFEESCRIPT_WORD2,					FALSE },
	{ "globalclass",			SCE_COFFEESCRIPT_GLOBALCLASS,			FALSE },
	{ "commentblock",			SCE_COFFEESCRIPT_COMMENTBLOCK,			FALSE },
	{ "verbose_regex",			SCE_COFFEESCRIPT_VERBOSE_REGEX,			FALSE },
	{ "verbose_regex_comment",	SCE_COFFEESCRIPT_VERBOSE_REGEX_COMMENT,	FALSE },
	{ "instanceproperty",		SCE_COFFEESCRIPT_INSTANCEPROPERTY,		FALSE }
};
static const HLKeyword highlighting_keywords_COFFEESCRIPT[] =
{
	{ "primary",		0,	FALSE },
	{ "secondary",		1,	FALSE },
	{ "globalclass",	3,	FALSE }
};
#define highlighting_properties_COFFEESCRIPT	EMPTY_PROPERTIES


/* CSS */
#define highlighting_lexer_CSS			SCLEX_CSS
static const HLStyle highlighting_styles_CSS[] =
{
	{ "default",				SCE_CSS_DEFAULT,					FALSE },
	{ "comment",				SCE_CSS_COMMENT,					FALSE },
	{ "tag",					SCE_CSS_TAG,						FALSE },
	{ "class",					SCE_CSS_CLASS,						FALSE },
	{ "pseudoclass",			SCE_CSS_PSEUDOCLASS,				FALSE },
	{ "unknown_pseudoclass",	SCE_CSS_UNKNOWN_PSEUDOCLASS,		FALSE },
	{ "unknown_identifier",		SCE_CSS_UNKNOWN_IDENTIFIER,			FALSE },
	{ "operator",				SCE_CSS_OPERATOR,					FALSE },
	{ "identifier",				SCE_CSS_IDENTIFIER,					FALSE },
	{ "doublestring",			SCE_CSS_DOUBLESTRING,				FALSE },
	{ "singlestring",			SCE_CSS_SINGLESTRING,				FALSE },
	{ "attribute",				SCE_CSS_ATTRIBUTE,					FALSE },
	{ "value",					SCE_CSS_VALUE,						FALSE },
	{ "id",						SCE_CSS_ID,							FALSE },
	{ "identifier2",			SCE_CSS_IDENTIFIER2,				FALSE },
	{ "variable",				SCE_CSS_VARIABLE,					FALSE },
	{ "important",				SCE_CSS_IMPORTANT,					FALSE },
	{ "directive",				SCE_CSS_DIRECTIVE,					FALSE },
	{ "identifier3",			SCE_CSS_IDENTIFIER3,				FALSE },
	{ "pseudoelement",			SCE_CSS_PSEUDOELEMENT,				FALSE },
	{ "extended_identifier",	SCE_CSS_EXTENDED_IDENTIFIER,		FALSE },
	{ "extended_pseudoclass",	SCE_CSS_EXTENDED_PSEUDOCLASS,		FALSE },
	{ "extended_pseudoelement",	SCE_CSS_EXTENDED_PSEUDOELEMENT,		FALSE },
	{ "media",					SCE_CSS_MEDIA,						FALSE }
};
static const HLKeyword highlighting_keywords_CSS[] =
{
	{ "primary",					0,	FALSE },
	{ "pseudoclasses",				1,	FALSE },
	{ "secondary",					2,	FALSE },
	{ "css3_properties",			3,	FALSE },
	{ "pseudo_elements",			4,	FALSE },
	{ "browser_css_properties",		5 ,	FALSE },
	{ "browser_pseudo_classes",		6,	FALSE },
	{ "browser_pseudo_elements",	7,	FALSE }
};
#define highlighting_properties_CSS		EMPTY_PROPERTIES


/* Cobol */
#define highlighting_lexer_COBOL		SCLEX_COBOL
#define highlighting_styles_COBOL		highlighting_styles_C
static const HLKeyword highlighting_keywords_COBOL[] =
{
	{ "primary",			0,	FALSE },
	{ "secondary",			1,	FALSE },
	{ "extended_keywords",	2,	FALSE }
};
#define highlighting_properties_COBOL	highlighting_properties_C


/* Conf */
#define highlighting_lexer_CONF			SCLEX_PROPERTIES
static const HLStyle highlighting_styles_CONF[] =
{
	{ "default",	SCE_PROPS_DEFAULT,		FALSE },
	{ "comment",	SCE_PROPS_COMMENT,		FALSE },
	{ "section",	SCE_PROPS_SECTION,		FALSE },
	{ "key",		SCE_PROPS_KEY,			FALSE },
	{ "assignment",	SCE_PROPS_ASSIGNMENT,	FALSE },
	{ "defval",		SCE_PROPS_DEFVAL,		FALSE }
};
#define highlighting_keywords_CONF		EMPTY_KEYWORDS
#define highlighting_properties_CONF	EMPTY_PROPERTIES


/* D */
#define highlighting_lexer_D		SCLEX_D
static const HLStyle highlighting_styles_D[] =
{
	{ "default",					SCE_D_DEFAULT,						FALSE },
	{ "comment",					SCE_D_COMMENT,						FALSE },
	{ "commentline",				SCE_D_COMMENTLINE,					FALSE },
	{ "commentdoc",					SCE_D_COMMENTDOC,					FALSE },
	{ "commentnested",				SCE_D_COMMENTNESTED,				FALSE },
	{ "number",						SCE_D_NUMBER,						FALSE },
	{ "word",						SCE_D_WORD,							FALSE },
	{ "word2",						SCE_D_WORD2,						FALSE },
	{ "word3",						SCE_D_WORD3,						FALSE },
	{ "typedef",					SCE_D_TYPEDEF,						FALSE }, /* FIXME: don't remap here */
	{ "typedef",					SCE_D_WORD5,						FALSE },
	{ "string",						SCE_D_STRING,						FALSE },
	{ "string",						SCE_D_STRINGB,						FALSE },
	{ "string",						SCE_D_STRINGR,						FALSE },
	{ "stringeol",					SCE_D_STRINGEOL,					FALSE },
	{ "character",					SCE_D_CHARACTER,					FALSE },
	{ "operator",					SCE_D_OPERATOR,						FALSE },
	{ "identifier",					SCE_D_IDENTIFIER,					FALSE },
	{ "commentlinedoc",				SCE_D_COMMENTLINEDOC,				FALSE },
	{ "commentdockeyword",			SCE_D_COMMENTDOCKEYWORD,			FALSE },
	{ "commentdockeyworderror",		SCE_D_COMMENTDOCKEYWORDERROR,		FALSE }
	/* these are for user-defined keywords we don't set yet */
	/*{ "word6",					SCE_D_WORD6,						FALSE },
	{ "word7",						SCE_D_WORD7,						FALSE }*/
};
static const HLKeyword highlighting_keywords_D[] =
{
	{ "primary",	0,	FALSE },
	/* SCI_SETKEYWORDS = 1 - secondary + global tags file types */
	{ "secondary",	1,	TRUE },
	{ "docComment",	2,	FALSE },
	/* SCI_SETKEYWORDS = 3 is for current session types - see document_highlight_tags() */
	{ "types",		4,	FALSE },
};
#define highlighting_properties_D		EMPTY_PROPERTIES


/* Diff */
#define highlighting_lexer_DIFF			SCLEX_DIFF
static const HLStyle highlighting_styles_DIFF[] =
{
	{ "default",					SCE_DIFF_DEFAULT,					FALSE },
	{ "comment",					SCE_DIFF_COMMENT,					FALSE },
	{ "command",					SCE_DIFF_COMMAND,					FALSE },
	{ "header",						SCE_DIFF_HEADER,					FALSE },
	{ "position",					SCE_DIFF_POSITION,					FALSE },
	{ "deleted",					SCE_DIFF_DELETED,					FALSE },
	{ "added",						SCE_DIFF_ADDED,						FALSE },
	{ "changed",					SCE_DIFF_CHANGED,					FALSE },
	{ "patch_add",					SCE_DIFF_PATCH_ADD,					FALSE },
	{ "patch_delete",				SCE_DIFF_PATCH_DELETE,				FALSE },
	{ "removed_patch_add",			SCE_DIFF_REMOVED_PATCH_ADD,			FALSE },
	{ "removed_patch_delete",		SCE_DIFF_REMOVED_PATCH_DELETE,		FALSE }
};
#define highlighting_keywords_DIFF		EMPTY_KEYWORDS
#define highlighting_properties_DIFF	EMPTY_PROPERTIES


#define highlighting_lexer_DOCBOOK			SCLEX_XML
static const HLStyle highlighting_styles_DOCBOOK[] =
{
	{ "default",				SCE_H_DEFAULT,					FALSE	 },
	{ "tag",					SCE_H_TAG,						FALSE	 },
	{ "tagunknown",				SCE_H_TAGUNKNOWN,				FALSE	 },
	{ "attribute",				SCE_H_ATTRIBUTE,				FALSE	 },
	{ "attributeunknown",		SCE_H_ATTRIBUTEUNKNOWN,			FALSE	 },
	{ "number",					SCE_H_NUMBER,					FALSE	 },
	{ "doublestring",			SCE_H_DOUBLESTRING,				FALSE	 },
	{ "singlestring",			SCE_H_SINGLESTRING,				FALSE	 },
	{ "other",					SCE_H_OTHER,					FALSE	 },
	{ "comment",				SCE_H_COMMENT,					FALSE	 },
	{ "entity",					SCE_H_ENTITY,					FALSE	 },
	{ "tagend",					SCE_H_TAGEND,					FALSE	 },
	{ "xmlstart",				SCE_H_XMLSTART,					TRUE	 },
	{ "xmlend",					SCE_H_XMLEND,					FALSE	 },
	{ "cdata",					SCE_H_CDATA,					FALSE	 },
	{ "question",				SCE_H_QUESTION,					FALSE	 },
	{ "value",					SCE_H_VALUE,					FALSE	 },
	{ "xccomment",				SCE_H_XCCOMMENT,				FALSE	 },
	{ "sgml_default",			SCE_H_SGML_DEFAULT,				FALSE	 },
	{ "sgml_comment",			SCE_H_SGML_COMMENT,				FALSE	 },
	{ "sgml_special",			SCE_H_SGML_SPECIAL,				FALSE	 },
	{ "sgml_command",			SCE_H_SGML_COMMAND,				FALSE	 },
	{ "sgml_doublestring",		SCE_H_SGML_DOUBLESTRING,		FALSE	 },
	{ "sgml_simplestring",		SCE_H_SGML_SIMPLESTRING,		FALSE	 },
	{ "sgml_1st_param",			SCE_H_SGML_1ST_PARAM,			FALSE	 },
	{ "sgml_entity",			SCE_H_SGML_ENTITY,				FALSE	 },
	{ "sgml_block_default",		SCE_H_SGML_BLOCK_DEFAULT,		FALSE	 },
	{ "sgml_1st_param_comment",	SCE_H_SGML_1ST_PARAM_COMMENT,	FALSE	 },
	{ "sgml_error",				SCE_H_SGML_ERROR,				FALSE	 }
};
static const HLKeyword highlighting_keywords_DOCBOOK[] =
{
	{ "elements",	0,	FALSE },
	{ "dtd",		5,	FALSE }
};
#define highlighting_properties_DOCBOOK		EMPTY_PROPERTIES


/* Erlang */
#define highlighting_lexer_ERLANG		SCLEX_ERLANG
static const HLStyle highlighting_styles_ERLANG[] =
{
	{ "default",				SCE_ERLANG_DEFAULT,					FALSE },
	{ "comment",				SCE_ERLANG_COMMENT,					FALSE },
	{ "variable",				SCE_ERLANG_VARIABLE,				FALSE },
	{ "number",					SCE_ERLANG_NUMBER,					FALSE },
	{ "keyword",				SCE_ERLANG_KEYWORD,					FALSE },
	{ "string",					SCE_ERLANG_STRING,					FALSE },
	{ "operator",				SCE_ERLANG_OPERATOR,				FALSE },
	{ "atom",					SCE_ERLANG_ATOM,					FALSE },
	{ "function_name",			SCE_ERLANG_FUNCTION_NAME,			FALSE },
	{ "character",				SCE_ERLANG_CHARACTER,				FALSE },
	{ "macro",					SCE_ERLANG_MACRO,					FALSE },
	{ "record",					SCE_ERLANG_RECORD,					FALSE },
	{ "preproc",				SCE_ERLANG_PREPROC,					FALSE },
	{ "node_name",				SCE_ERLANG_NODE_NAME,				FALSE },
	{ "comment_function",		SCE_ERLANG_COMMENT_FUNCTION,		FALSE },
	{ "comment_module",			SCE_ERLANG_COMMENT_MODULE,			FALSE },
	{ "comment_doc",			SCE_ERLANG_COMMENT_DOC,				FALSE },
	{ "comment_doc_macro",		SCE_ERLANG_COMMENT_DOC_MACRO,		FALSE },
	{ "atom_quoted",			SCE_ERLANG_ATOM_QUOTED,				FALSE },
	{ "macro_quoted",			SCE_ERLANG_MACRO_QUOTED,			FALSE },
	{ "record_quoted",			SCE_ERLANG_RECORD_QUOTED,			FALSE },
	{ "node_name_quoted",		SCE_ERLANG_NODE_NAME_QUOTED,		FALSE },
	{ "bifs",					SCE_ERLANG_BIFS,					FALSE },
	{ "modules",				SCE_ERLANG_MODULES,					FALSE },
	{ "modules_att",			SCE_ERLANG_MODULES_ATT,				FALSE },
	{ "unknown",				SCE_ERLANG_UNKNOWN,					FALSE }
};
static const HLKeyword highlighting_keywords_ERLANG[] =
{
	{ "keywords",	0,		FALSE },
	{ "bifs",		1,		FALSE },
	{ "preproc",	2,		FALSE },
	{ "module",		3,		FALSE },
	{ "doc",		4,		FALSE },
	{ "doc_macro",	5,		FALSE }
};
#define highlighting_properties_ERLANG	EMPTY_PROPERTIES


/* F77 */
#define highlighting_lexer_F77			SCLEX_F77
static const HLStyle highlighting_styles_F77[] =
{
	{ "default",		SCE_F_DEFAULT,			FALSE },
	{ "comment",		SCE_F_COMMENT,			FALSE },
	{ "number",			SCE_F_NUMBER,			FALSE },
	{ "string",			SCE_F_STRING1,			FALSE },
	{ "operator",		SCE_F_OPERATOR,			FALSE },
	{ "identifier",		SCE_F_IDENTIFIER,		FALSE },
	{ "string2",		SCE_F_STRING2,			FALSE },
	{ "word",			SCE_F_WORD,				FALSE },
	{ "word2",			SCE_F_WORD2,			FALSE },
	{ "word3",			SCE_F_WORD3,			FALSE },
	{ "preprocessor",	SCE_F_PREPROCESSOR,		FALSE },
	{ "operator2",		SCE_F_OPERATOR2,		FALSE },
	{ "continuation",	SCE_F_CONTINUATION,		FALSE },
	{ "stringeol",		SCE_F_STRINGEOL,		FALSE },
	{ "label",			SCE_F_LABEL,			FALSE }
};
static const HLKeyword highlighting_keywords_F77[] =
{
	{ "primary",				0,	FALSE },
	{ "intrinsic_functions",	1,	FALSE },
	{ "user_functions",			2,	FALSE }
};
#define highlighting_properties_F77		EMPTY_PROPERTIES


/* Ferite */
#define highlighting_lexer_FERITE		SCLEX_CPP
#define highlighting_styles_FERITE		highlighting_styles_C
static const HLKeyword highlighting_keywords_FERITE[] =
{
	{ "primary",	0,	FALSE },
	{ "types",		1,	FALSE },
	{ "docComment", 2,	FALSE }
};
#define highlighting_properties_FERITE	highlighting_properties_C


/* Forth */
#define highlighting_lexer_FORTH		SCLEX_FORTH
static const HLStyle highlighting_styles_FORTH[] =
{
	{ "default",	SCE_FORTH_DEFAULT,		FALSE },
	{ "comment",	SCE_FORTH_COMMENT,		FALSE },
	{ "commentml",	SCE_FORTH_COMMENT_ML,	FALSE },
	{ "identifier",	SCE_FORTH_IDENTIFIER,	FALSE },
	{ "control",	SCE_FORTH_CONTROL,		FALSE },
	{ "keyword",	SCE_FORTH_KEYWORD,		FALSE },
	{ "defword",	SCE_FORTH_DEFWORD,		FALSE },
	{ "preword1",	SCE_FORTH_PREWORD1,		FALSE },
	{ "preword2",	SCE_FORTH_PREWORD2,		FALSE },
	{ "number",		SCE_FORTH_NUMBER,		FALSE },
	{ "string",		SCE_FORTH_STRING,		FALSE },
	{ "locale",		SCE_FORTH_LOCALE,		FALSE }
};
static const HLKeyword highlighting_keywords_FORTH[] =
{
	{ "primary",	0,	FALSE },
	{ "keyword",	1,	FALSE },
	{ "defword",	2,	FALSE },
	{ "preword1",	3,	FALSE },
	{ "preword2",	4,	FALSE },
	{ "string",		5,	FALSE }
};
#define highlighting_properties_FORTH	EMPTY_PROPERTIES


/* Fortran */
/* F77 and Fortran (F9x) uses different lexers but shares styles and keywords */
#define highlighting_lexer_FORTRAN			SCLEX_FORTRAN
#define highlighting_styles_FORTRAN			highlighting_styles_F77
#define highlighting_keywords_FORTRAN		highlighting_keywords_F77
#define highlighting_properties_FORTRAN		highlighting_properties_F77


/* Go */
#define highlighting_lexer_GO		SCLEX_CPP
#define highlighting_styles_GO		highlighting_styles_C
#define highlighting_keywords_GO	highlighting_keywords_C
#define highlighting_properties_GO	highlighting_properties_C


/* Haskell */
#define highlighting_lexer_HASKELL			SCLEX_HASKELL
static const HLStyle highlighting_styles_HASKELL[] =
{
	{ "default",				SCE_HA_DEFAULT,					FALSE },
	{ "commentline",			SCE_HA_COMMENTLINE,				FALSE },
	{ "commentblock",			SCE_HA_COMMENTBLOCK,			FALSE },
	{ "commentblock2",			SCE_HA_COMMENTBLOCK2,			FALSE },
	{ "commentblock3",			SCE_HA_COMMENTBLOCK3,			FALSE },
	{ "number",					SCE_HA_NUMBER,					FALSE },
	{ "keyword",				SCE_HA_KEYWORD,					FALSE },
	{ "import",					SCE_HA_IMPORT,					FALSE },
	{ "string",					SCE_HA_STRING,					FALSE },
	{ "character",				SCE_HA_CHARACTER,				FALSE },
	{ "class",					SCE_HA_CLASS,					FALSE },
	{ "operator",				SCE_HA_OPERATOR,				FALSE },
	{ "identifier",				SCE_HA_IDENTIFIER,				FALSE },
	{ "instance",				SCE_HA_INSTANCE,				FALSE },
	{ "capital",				SCE_HA_CAPITAL,					FALSE },
	{ "module",					SCE_HA_MODULE,					FALSE },
	{ "data",					SCE_HA_DATA,					FALSE },
	{ "pragma",					SCE_HA_PRAGMA,					FALSE },
	{ "preprocessor",			SCE_HA_PREPROCESSOR,			FALSE },
	{ "stringeol",				SCE_HA_STRINGEOL,				FALSE },
	{ "reserved_operator",		SCE_HA_RESERVED_OPERATOR,		FALSE },
	{ "literate_comment",		SCE_HA_LITERATE_COMMENT,		FALSE },
	{ "literate_codedelim",		SCE_HA_LITERATE_CODEDELIM,		FALSE }
};
static const HLKeyword highlighting_keywords_HASKELL[] =
{
	{ "keywords",			0,		FALSE },
	{ "ffi",				1,		FALSE },
	{ "reserved_operators",	2,		FALSE }
};
#define highlighting_properties_HASKELL		EMPTY_PROPERTIES


/* HAXE */
#define highlighting_lexer_HAXE			SCLEX_CPP
#define highlighting_styles_HAXE		highlighting_styles_C
static const HLKeyword highlighting_keywords_HAXE[] =
{
	{ "primary",		0,	FALSE },
	{ "secondary",		1,	FALSE },
	{ "classes",		3,	FALSE }
};
#define highlighting_properties_HAXE	highlighting_properties_C


/* HTML */
#define highlighting_lexer_HTML		SCLEX_HTML
static const HLStyle highlighting_styles_HTML[] =
{
	{ "html_default",				SCE_H_DEFAULT,					FALSE	 },
	{ "html_tag",					SCE_H_TAG,						FALSE	 },
	{ "html_tagunknown",			SCE_H_TAGUNKNOWN,				FALSE	 },
	{ "html_attribute",				SCE_H_ATTRIBUTE,				FALSE	 },
	{ "html_attributeunknown",		SCE_H_ATTRIBUTEUNKNOWN,			FALSE	 },
	{ "html_number",				SCE_H_NUMBER,					FALSE	 },
	{ "html_doublestring",			SCE_H_DOUBLESTRING,				FALSE	 },
	{ "html_singlestring",			SCE_H_SINGLESTRING,				FALSE	 },
	{ "html_other",					SCE_H_OTHER,					FALSE	 },
	{ "html_comment",				SCE_H_COMMENT,					FALSE	 },
	{ "html_entity",				SCE_H_ENTITY,					FALSE	 },
	{ "html_tagend",				SCE_H_TAGEND,					FALSE	 },
	{ "html_xmlstart",				SCE_H_XMLSTART,					TRUE	 },
	{ "html_xmlend",				SCE_H_XMLEND,					FALSE	 },
	{ "html_script",				SCE_H_SCRIPT,					FALSE	 },
	{ "html_asp",					SCE_H_ASP,						TRUE	 },
	{ "html_aspat",					SCE_H_ASPAT,					TRUE	 },
	{ "html_cdata",					SCE_H_CDATA,					FALSE	 },
	{ "html_question",				SCE_H_QUESTION,					FALSE	 },
	{ "html_value",					SCE_H_VALUE,					FALSE	 },
	{ "html_xccomment",				SCE_H_XCCOMMENT,				FALSE	 },

	{ "sgml_default",				SCE_H_SGML_DEFAULT,				FALSE	 },
	{ "sgml_comment",				SCE_H_SGML_COMMENT,				FALSE	 },
	{ "sgml_special",				SCE_H_SGML_SPECIAL,				FALSE	 },
	{ "sgml_command",				SCE_H_SGML_COMMAND,				FALSE	 },
	{ "sgml_doublestring",			SCE_H_SGML_DOUBLESTRING,		FALSE	 },
	{ "sgml_simplestring",			SCE_H_SGML_SIMPLESTRING,		FALSE	 },
	{ "sgml_1st_param",				SCE_H_SGML_1ST_PARAM,			FALSE	 },
	{ "sgml_entity",				SCE_H_SGML_ENTITY,				FALSE	 },
	{ "sgml_block_default",			SCE_H_SGML_BLOCK_DEFAULT,		FALSE	 },
	{ "sgml_1st_param_comment",		SCE_H_SGML_1ST_PARAM_COMMENT,	FALSE	 },
	{ "sgml_error",					SCE_H_SGML_ERROR,				FALSE	 },

	/* embedded JavaScript */
	{ "jscript_start",				SCE_HJ_START,					FALSE	 },
	{ "jscript_default",			SCE_HJ_DEFAULT,					FALSE	 },
	{ "jscript_comment",			SCE_HJ_COMMENT,					FALSE	 },
	{ "jscript_commentline",		SCE_HJ_COMMENTLINE,				FALSE	 },
	{ "jscript_commentdoc",			SCE_HJ_COMMENTDOC,				FALSE	 },
	{ "jscript_number",				SCE_HJ_NUMBER,					FALSE	 },
	{ "jscript_word",				SCE_HJ_WORD,					FALSE	 },
	{ "jscript_keyword",			SCE_HJ_KEYWORD,					FALSE	 },
	{ "jscript_doublestring",		SCE_HJ_DOUBLESTRING,			FALSE	 },
	{ "jscript_singlestring",		SCE_HJ_SINGLESTRING,			FALSE	 },
	{ "jscript_symbols",			SCE_HJ_SYMBOLS,					FALSE	 },
	{ "jscript_stringeol",			SCE_HJ_STRINGEOL,				FALSE	 },
	{ "jscript_regex",				SCE_HJ_REGEX,					FALSE	 },

	/* for HB, VBScript?, use the same styles as for JavaScript */
	{ "jscript_start",				SCE_HB_START,					FALSE	 },
	{ "jscript_default",			SCE_HB_DEFAULT,					FALSE	 },
	{ "jscript_commentline",		SCE_HB_COMMENTLINE,				FALSE	 },
	{ "jscript_number",				SCE_HB_NUMBER,					FALSE	 },
	{ "jscript_keyword",			SCE_HB_WORD,					FALSE	 }, /* keywords */
	{ "jscript_doublestring",		SCE_HB_STRING,					FALSE	 },
	{ "jscript_symbols",			SCE_HB_IDENTIFIER,				FALSE	 },
	{ "jscript_stringeol",			SCE_HB_STRINGEOL,				FALSE	 },

	/* for HBA, VBScript?, use the same styles as for JavaScript */
	{ "jscript_start",				SCE_HBA_START,					FALSE	 },
	{ "jscript_default",			SCE_HBA_DEFAULT,				FALSE	 },
	{ "jscript_commentline",		SCE_HBA_COMMENTLINE,			FALSE	 },
	{ "jscript_number",				SCE_HBA_NUMBER,					FALSE	 },
	{ "jscript_keyword",			SCE_HBA_WORD,					FALSE	 }, /* keywords */
	{ "jscript_doublestring",		SCE_HBA_STRING,					FALSE	 },
	{ "jscript_symbols",			SCE_HBA_IDENTIFIER,				FALSE	 },
	{ "jscript_stringeol",			SCE_HBA_STRINGEOL,				FALSE	 },

	/* for HJA, ASP Javascript, use the same styles as for JavaScript */
	{ "jscript_start",				SCE_HJA_START,					FALSE	 },
	{ "jscript_default",			SCE_HJA_DEFAULT,				FALSE	 },
	{ "jscript_comment",			SCE_HJA_COMMENT,				FALSE	 },
	{ "jscript_commentline",		SCE_HJA_COMMENTLINE,			FALSE	 },
	{ "jscript_commentdoc",			SCE_HJA_COMMENTDOC,				FALSE	 },
	{ "jscript_number",				SCE_HJA_NUMBER,					FALSE	 },
	{ "jscript_word",				SCE_HJA_WORD,					FALSE	 },
	{ "jscript_keyword",			SCE_HJA_KEYWORD,				FALSE	 },
	{ "jscript_doublestring",		SCE_HJA_DOUBLESTRING,			FALSE	 },
	{ "jscript_singlestring",		SCE_HJA_SINGLESTRING,			FALSE	 },
	{ "jscript_symbols",			SCE_HJA_SYMBOLS,				FALSE	 },
	{ "jscript_stringeol",			SCE_HJA_STRINGEOL,				FALSE	 },
	{ "jscript_regex",				SCE_HJA_REGEX,					FALSE	 },

	/* embedded Python */
	{ "jscript_start",				SCE_HP_START,					FALSE	 },
	{ "python_default",				SCE_HP_DEFAULT,					FALSE	 },
	{ "python_commentline",			SCE_HP_COMMENTLINE,				FALSE	 },
	{ "python_number",				SCE_HP_NUMBER,					FALSE	 },
	{ "python_string",				SCE_HP_STRING,					FALSE	 },
	{ "python_character",			SCE_HP_CHARACTER,				FALSE	 },
	{ "python_word",				SCE_HP_WORD,					FALSE	 },
	{ "python_triple",				SCE_HP_TRIPLE,					FALSE	 },
	{ "python_tripledouble",		SCE_HP_TRIPLEDOUBLE,			FALSE	 },
	{ "python_classname",			SCE_HP_CLASSNAME,				FALSE	 },
	{ "python_defname",				SCE_HP_DEFNAME,					FALSE	 },
	{ "python_operator",			SCE_HP_OPERATOR,				FALSE	 },
	{ "python_identifier",			SCE_HP_IDENTIFIER,				FALSE	 },

	/* for embedded HPA (what is this?) we use the Python styles */
	{ "jscript_start",				SCE_HPA_START,					FALSE	 },
	{ "python_default",				SCE_HPA_DEFAULT,				FALSE	 },
	{ "python_commentline",			SCE_HPA_COMMENTLINE,			FALSE	 },
	{ "python_number",				SCE_HPA_NUMBER,					FALSE	 },
	{ "python_string",				SCE_HPA_STRING,					FALSE	 },
	{ "python_character",			SCE_HPA_CHARACTER,				FALSE	 },
	{ "python_word",				SCE_HPA_WORD,					FALSE	 },
	{ "python_triple",				SCE_HPA_TRIPLE,					FALSE	 },
	{ "python_tripledouble",		SCE_HPA_TRIPLEDOUBLE,			FALSE	 },
	{ "python_classname",			SCE_HPA_CLASSNAME,				FALSE	 },
	{ "python_defname",				SCE_HPA_DEFNAME,				FALSE	 },
	{ "python_operator",			SCE_HPA_OPERATOR,				FALSE	 },
	{ "python_identifier",			SCE_HPA_IDENTIFIER,				FALSE	 },

	/* PHP */
	{ "php_default",				SCE_HPHP_DEFAULT,				FALSE	 },
	{ "php_simplestring",			SCE_HPHP_SIMPLESTRING,			FALSE	 },
	{ "php_hstring",				SCE_HPHP_HSTRING,				FALSE	 },
	{ "php_number",					SCE_HPHP_NUMBER,				FALSE	 },
	{ "php_word",					SCE_HPHP_WORD,					FALSE	 },
	{ "php_variable",				SCE_HPHP_VARIABLE,				FALSE	 },
	{ "php_comment",				SCE_HPHP_COMMENT,				FALSE	 },
	{ "php_commentline",			SCE_HPHP_COMMENTLINE,			FALSE	 },
	{ "php_operator",				SCE_HPHP_OPERATOR,				FALSE	 },
	{ "php_hstring_variable",		SCE_HPHP_HSTRING_VARIABLE,		FALSE	 },
	{ "php_complex_variable",		SCE_HPHP_COMPLEX_VARIABLE,		FALSE	 }
};
static const HLKeyword highlighting_keywords_HTML[] =
{
	{ "html",		0,	FALSE },
	{ "javascript", 1,	FALSE },
	{ "vbscript",	2,	FALSE },
	{ "python",		3,	FALSE },
	{ "php",		4,	FALSE },
	{ "sgml",		5,	FALSE }
};
static const HLProperty highlighting_properties_HTML[] =
{
	{ "fold.html",				"1" },
	{ "fold.html.preprocessor",	"0" }
};


/* Java */
#define highlighting_lexer_JAVA			SCLEX_CPP
#define highlighting_styles_JAVA		highlighting_styles_C
static const HLKeyword highlighting_keywords_JAVA[] =
{
	{ "primary",	0,	FALSE },
	/* SCI_SETKEYWORDS = 1 - secondary + global tags file types, see below */
	{ "secondary",	1,	TRUE },
	{ "doccomment",	2,	FALSE },
	/* SCI_SETKEYWORDS = 3 is for current session types - see document_highlight_tags() */
	{ "typedefs",	4,	FALSE }
};
#define highlighting_properties_JAVA	highlighting_properties_C


/* JavaScript */
#define highlighting_lexer_JS		SCLEX_CPP
#define highlighting_styles_JS		highlighting_styles_C
static const HLKeyword highlighting_keywords_JS[] =
{
	{ "primary",		0,	FALSE },
	{ "secondary",		1,	FALSE }
};
#define highlighting_properties_JS	highlighting_properties_C

/* Julia */
#define highlighting_lexer_JULIA		SCLEX_JULIA
static const HLStyle highlighting_styles_JULIA[] =
{
	{ "default",			SCE_JULIA_DEFAULT,			FALSE },
	{ "comment",			SCE_JULIA_COMMENT,			FALSE },
	{ "number",				SCE_JULIA_NUMBER,			FALSE },
	{ "keyword1",			SCE_JULIA_KEYWORD1,			FALSE },
	{ "keyword2",			SCE_JULIA_KEYWORD2,			FALSE },
	{ "keyword3",			SCE_JULIA_KEYWORD3,			FALSE },
	{ "char",				SCE_JULIA_CHAR,				FALSE },
	{ "operator",			SCE_JULIA_OPERATOR,			FALSE },
	{ "bracket",			SCE_JULIA_BRACKET,			FALSE },
	{ "identifier",			SCE_JULIA_IDENTIFIER,		FALSE },
	{ "string",				SCE_JULIA_STRING,			FALSE },
	{ "symbol",				SCE_JULIA_SYMBOL,			FALSE },
	{ "macro",				SCE_JULIA_MACRO,			FALSE },
	{ "stringinterp",		SCE_JULIA_STRINGINTERP,		FALSE },
	{ "docstring",			SCE_JULIA_DOCSTRING,		FALSE },
	{ "stringliteral",		SCE_JULIA_STRINGLITERAL,	FALSE },
	{ "command",			SCE_JULIA_COMMAND,			FALSE },
	{ "commandliteral",		SCE_JULIA_COMMANDLITERAL,	FALSE },
	{ "typeannotation",		SCE_JULIA_TYPEANNOT,		FALSE },
	{ "lexerror",			SCE_JULIA_LEXERROR,			FALSE },
	{ "keyword4",			SCE_JULIA_KEYWORD4,			FALSE },
	{ "typeoperator",		SCE_JULIA_TYPEOPERATOR,		FALSE },
};
static const HLKeyword highlighting_keywords_JULIA[] =
{
	{ "primary",		0,	FALSE },
	{ "secondary",		1,	FALSE },
	{ "tertiary",		2,	FALSE },
	{ "functions",		3,	FALSE }
};
#define highlighting_properties_JULIA	EMPTY_PROPERTIES


/* LaTeX */
#define highlighting_lexer_LATEX		SCLEX_LATEX
static const HLStyle highlighting_styles_LATEX[] =
{
	{ "default",		SCE_L_DEFAULT,		FALSE },
	{ "command",		SCE_L_COMMAND,		FALSE },
	{ "tag",			SCE_L_TAG,			FALSE },
	{ "math",			SCE_L_MATH,			FALSE },
	{ "comment",		SCE_L_COMMENT,		FALSE },
	{ "tag2",			SCE_L_TAG2,			FALSE },
	{ "math2",			SCE_L_MATH2,		FALSE },
	{ "comment2",		SCE_L_COMMENT2,		FALSE },
	{ "verbatim",		SCE_L_VERBATIM,		FALSE },
	{ "shortcmd",		SCE_L_SHORTCMD,		FALSE },
	{ "special",		SCE_L_SPECIAL,		FALSE },
	{ "cmdopt",			SCE_L_CMDOPT,		FALSE },
	{ "error",			SCE_L_ERROR,		FALSE }
};
static const HLKeyword highlighting_keywords_LATEX[] =
{
	{ "primary", 0,	FALSE }
};
#define highlighting_properties_LATEX	EMPTY_PROPERTIES


/* Lisp */
#define highlighting_lexer_LISP			SCLEX_LISP
static const HLStyle highlighting_styles_LISP[] =
{
	{ "default",			SCE_LISP_DEFAULT,			FALSE },
	{ "comment",			SCE_LISP_COMMENT,			FALSE },
	{ "multicomment",		SCE_LISP_MULTI_COMMENT,		FALSE },
	{ "number",				SCE_LISP_NUMBER,			FALSE },
	{ "keyword",			SCE_LISP_KEYWORD,			FALSE },
	{ "symbol",				SCE_LISP_SYMBOL,			FALSE },
	{ "string",				SCE_LISP_STRING,			FALSE },
	{ "stringeol",			SCE_LISP_STRINGEOL,			FALSE },
	{ "identifier",			SCE_LISP_IDENTIFIER,		FALSE },
	{ "operator",			SCE_LISP_OPERATOR,			FALSE },
	{ "special",			SCE_LISP_SPECIAL,			FALSE },
	{ "keywordkw",			SCE_LISP_KEYWORD_KW,		FALSE }
};
static const HLKeyword highlighting_keywords_LISP[] =
{
	{ "keywords",			0,	FALSE },
	{ "special_keywords",	1,	FALSE }
};
#define highlighting_properties_LISP	EMPTY_PROPERTIES


/* Lua */
#define highlighting_lexer_LUA			SCLEX_LUA
static const HLStyle highlighting_styles_LUA[] =
{
	{ "default",			SCE_LUA_DEFAULT,		FALSE },
	{ "comment",			SCE_LUA_COMMENT,		FALSE },
	{ "commentline",		SCE_LUA_COMMENTLINE,	FALSE },
	{ "commentdoc",			SCE_LUA_COMMENTDOC,		FALSE },
	{ "number",				SCE_LUA_NUMBER,			FALSE },
	{ "word",				SCE_LUA_WORD,			FALSE },
	{ "string",				SCE_LUA_STRING,			FALSE },
	{ "character",			SCE_LUA_CHARACTER,		FALSE },
	{ "literalstring",		SCE_LUA_LITERALSTRING,	FALSE },
	{ "preprocessor",		SCE_LUA_PREPROCESSOR,	FALSE },
	{ "operator",			SCE_LUA_OPERATOR,		FALSE },
	{ "identifier",			SCE_LUA_IDENTIFIER,		FALSE },
	{ "stringeol",			SCE_LUA_STRINGEOL,		FALSE },
	{ "function_basic",		SCE_LUA_WORD2,			FALSE },
	{ "function_other",		SCE_LUA_WORD3,			FALSE },
	{ "coroutines",			SCE_LUA_WORD4,			FALSE },
	{ "word5",				SCE_LUA_WORD5,			FALSE },
	{ "word6",				SCE_LUA_WORD6,			FALSE },
	{ "word7",				SCE_LUA_WORD7,			FALSE },
	{ "word8",				SCE_LUA_WORD8,			FALSE },
	{ "label",				SCE_LUA_LABEL,			FALSE }
};
static const HLKeyword highlighting_keywords_LUA[] =
{
	{ "keywords",			0,		FALSE },
	{ "function_basic",		1,		FALSE },
	{ "function_other",		2,		FALSE },
	{ "coroutines",			3,		FALSE },
	{ "user1",				4,		FALSE },
	{ "user2",				5,		FALSE },
	{ "user3",				6,		FALSE },
	{ "user4",				7,		FALSE }
};
#define highlighting_properties_LUA		EMPTY_PROPERTIES


/* Makefile */
#define highlighting_lexer_MAKE			SCLEX_MAKEFILE
static const HLStyle highlighting_styles_MAKE[] =
{
	{ "default",		SCE_MAKE_DEFAULT,			FALSE },
	{ "comment",		SCE_MAKE_COMMENT,			FALSE },
	{ "preprocessor",	SCE_MAKE_PREPROCESSOR,		FALSE },
	{ "identifier",		SCE_MAKE_IDENTIFIER,		FALSE },
	{ "operator",		SCE_MAKE_OPERATOR,			FALSE },
	{ "target",			SCE_MAKE_TARGET,			FALSE },
	{ "ideol",			SCE_MAKE_IDEOL,				FALSE }
};
#define highlighting_keywords_MAKE		EMPTY_KEYWORDS
#define highlighting_properties_MAKE	EMPTY_PROPERTIES


/* Markdown */
#define highlighting_lexer_MARKDOWN			SCLEX_MARKDOWN
static const HLStyle highlighting_styles_MARKDOWN[] =
{
	{ "default",		SCE_MARKDOWN_DEFAULT,			FALSE },
	{ "default",		SCE_MARKDOWN_LINE_BEGIN,		FALSE }, /* FIXME: avoid in-code re-mappings */
	{ "default",		SCE_MARKDOWN_PRECHAR,			FALSE },
	{ "strong",			SCE_MARKDOWN_STRONG1,			FALSE },
	{ "strong",			SCE_MARKDOWN_STRONG2,			FALSE },
	{ "emphasis",		SCE_MARKDOWN_EM1,				FALSE },
	{ "emphasis",		SCE_MARKDOWN_EM2,				FALSE },
	{ "header1",		SCE_MARKDOWN_HEADER1,			FALSE },
	{ "header2",		SCE_MARKDOWN_HEADER2,			FALSE },
	{ "header3",		SCE_MARKDOWN_HEADER3,			FALSE },
	{ "header4",		SCE_MARKDOWN_HEADER4,			FALSE },
	{ "header5",		SCE_MARKDOWN_HEADER5,			FALSE },
	{ "header6",		SCE_MARKDOWN_HEADER6,			FALSE },
	{ "ulist_item",		SCE_MARKDOWN_ULIST_ITEM,		FALSE },
	{ "olist_item",		SCE_MARKDOWN_OLIST_ITEM,		FALSE },
	{ "blockquote",		SCE_MARKDOWN_BLOCKQUOTE,		FALSE },
	{ "strikeout",		SCE_MARKDOWN_STRIKEOUT,			FALSE },
	{ "hrule",			SCE_MARKDOWN_HRULE,				FALSE },
	{ "link",			SCE_MARKDOWN_LINK,				FALSE },
	{ "code",			SCE_MARKDOWN_CODE,				FALSE },
	{ "code",			SCE_MARKDOWN_CODE2,				FALSE },
	{ "codebk",			SCE_MARKDOWN_CODEBK,			FALSE }
};
#define highlighting_keywords_MARKDOWN		EMPTY_KEYWORDS
#define highlighting_properties_MARKDOWN	EMPTY_PROPERTIES


/* Matlab */
#define highlighting_lexer_MATLAB		SCLEX_OCTAVE /* not MATLAB to support Octave's # comments */
static const HLStyle highlighting_styles_MATLAB[] =
{
	{ "default",			SCE_MATLAB_DEFAULT,				FALSE },
	{ "comment",			SCE_MATLAB_COMMENT,				FALSE },
	{ "command",			SCE_MATLAB_COMMAND,				FALSE },
	{ "number",				SCE_MATLAB_NUMBER,				FALSE },
	{ "keyword",			SCE_MATLAB_KEYWORD,				FALSE },
	{ "string",				SCE_MATLAB_STRING,				FALSE },
	{ "operator",			SCE_MATLAB_OPERATOR,			FALSE },
	{ "identifier",			SCE_MATLAB_IDENTIFIER,			FALSE },
	{ "doublequotedstring",	SCE_MATLAB_DOUBLEQUOTESTRING,	FALSE }
};
static const HLKeyword highlighting_keywords_MATLAB[] =
{
	{ "primary", 0,	FALSE }
};
#define highlighting_properties_MATLAB	EMPTY_PROPERTIES


/* NSIS */
#define highlighting_lexer_NSIS			SCLEX_NSIS
static const HLStyle highlighting_styles_NSIS[] =
{
	{ "default",			SCE_NSIS_DEFAULT,			FALSE },
	{ "comment",			SCE_NSIS_COMMENT,			FALSE },
	{ "stringdq",			SCE_NSIS_STRINGDQ,			FALSE },
	{ "stringlq",			SCE_NSIS_STRINGLQ,			FALSE },
	{ "stringrq",			SCE_NSIS_STRINGRQ,			FALSE },
	{ "function",			SCE_NSIS_FUNCTION,			FALSE },
	{ "variable",			SCE_NSIS_VARIABLE,			FALSE },
	{ "label",				SCE_NSIS_LABEL,				FALSE },
	{ "userdefined",		SCE_NSIS_USERDEFINED,		FALSE },
	{ "sectiondef",			SCE_NSIS_SECTIONDEF,		FALSE },
	{ "subsectiondef",		SCE_NSIS_SUBSECTIONDEF,		FALSE },
	{ "ifdefinedef",		SCE_NSIS_IFDEFINEDEF,		FALSE },
	{ "macrodef",			SCE_NSIS_MACRODEF,			FALSE },
	{ "stringvar",			SCE_NSIS_STRINGVAR,			FALSE },
	{ "number",				SCE_NSIS_NUMBER,			FALSE },
	{ "sectiongroup",		SCE_NSIS_SECTIONGROUP,		FALSE },
	{ "pageex",				SCE_NSIS_PAGEEX,			FALSE },
	{ "functiondef",		SCE_NSIS_FUNCTIONDEF,		FALSE },
	{ "commentbox",			SCE_NSIS_COMMENTBOX,		FALSE }
};
static const HLKeyword highlighting_keywords_NSIS[] =
{
	{ "functions",			0,	FALSE },
	{ "variables",			1,	FALSE },
	{ "lables",				2,	FALSE },
	{ "userdefined",		3,	FALSE }
};
#define highlighting_properties_NSIS	EMPTY_PROPERTIES


/* Objective-C */
#define highlighting_lexer_OBJECTIVEC		highlighting_lexer_C
#define highlighting_styles_OBJECTIVEC		highlighting_styles_C
static const HLKeyword highlighting_keywords_OBJECTIVEC[] =
{
	{ "primary",	0,	FALSE },
	/* SCI_SETKEYWORDS = 1 - secondary + global tags file types, see below */
	{ "secondary",	1,	TRUE },
	{ "docComment",	2,	FALSE }
	/* SCI_SETKEYWORDS = 3 is for current session types - see document_highlight_tags() */
};
#define highlighting_properties_OBJECTIVEC	highlighting_properties_C


/* Pascal */
#define highlighting_lexer_PASCAL		SCLEX_PASCAL
static const HLStyle highlighting_styles_PASCAL[] =
{
	{ "default",			SCE_PAS_DEFAULT,			FALSE },
	{ "identifier",			SCE_PAS_IDENTIFIER,			FALSE },
	{ "comment",			SCE_PAS_COMMENT,			FALSE },
	{ "comment2",			SCE_PAS_COMMENT2,			FALSE },
	{ "commentline",		SCE_PAS_COMMENTLINE,		FALSE },
	{ "preprocessor",		SCE_PAS_PREPROCESSOR,		FALSE },
	{ "preprocessor2",		SCE_PAS_PREPROCESSOR2,		FALSE },
	{ "number",				SCE_PAS_NUMBER,				FALSE },
	{ "hexnumber",			SCE_PAS_HEXNUMBER,			FALSE },
	{ "word",				SCE_PAS_WORD,				FALSE },
	{ "string",				SCE_PAS_STRING,				FALSE },
	{ "stringeol",			SCE_PAS_STRINGEOL,			FALSE },
	{ "character",			SCE_PAS_CHARACTER,			FALSE },
	{ "operator",			SCE_PAS_OPERATOR,			FALSE },
	{ "asm",				SCE_PAS_ASM,				FALSE }
};
static const HLKeyword highlighting_keywords_PASCAL[] =
{
	{ "primary", 0,	FALSE }
};
#define highlighting_properties_PASCAL	EMPTY_PROPERTIES


/* Perl */
#define highlighting_lexer_PERL			SCLEX_PERL
static const HLStyle highlighting_styles_PERL[] =
{
	{ "default",			SCE_PL_DEFAULT,				FALSE },
	{ "error",				SCE_PL_ERROR,				FALSE },
	{ "commentline",		SCE_PL_COMMENTLINE,			FALSE },
	{ "number",				SCE_PL_NUMBER,				FALSE },
	{ "word",				SCE_PL_WORD,				FALSE },
	{ "string",				SCE_PL_STRING,				FALSE },
	{ "character",			SCE_PL_CHARACTER,			FALSE },
	{ "preprocessor",		SCE_PL_PREPROCESSOR,		FALSE },
	{ "operator",			SCE_PL_OPERATOR,			FALSE },
	{ "identifier",			SCE_PL_IDENTIFIER,			FALSE },
	{ "scalar",				SCE_PL_SCALAR,				FALSE },
	{ "pod",				SCE_PL_POD,					FALSE },
	{ "regex",				SCE_PL_REGEX,				FALSE },
	{ "array",				SCE_PL_ARRAY,				FALSE },
	{ "hash",				SCE_PL_HASH,				FALSE },
	{ "symboltable",		SCE_PL_SYMBOLTABLE,			FALSE },
	{ "backticks",			SCE_PL_BACKTICKS,			FALSE },
	{ "pod_verbatim",		SCE_PL_POD_VERB,			FALSE },
	{ "reg_subst",			SCE_PL_REGSUBST,			FALSE },
	{ "datasection",		SCE_PL_DATASECTION,			FALSE },
	{ "here_delim",			SCE_PL_HERE_DELIM,			FALSE },
	{ "here_q",				SCE_PL_HERE_Q,				FALSE },
	{ "here_qq",			SCE_PL_HERE_QQ,				FALSE },
	{ "here_qx",			SCE_PL_HERE_QX,				FALSE },
	{ "string_q",			SCE_PL_STRING_Q,			FALSE },
	{ "string_qq",			SCE_PL_STRING_QQ,			FALSE },
	{ "string_qx",			SCE_PL_STRING_QX,			FALSE },
	{ "string_qr",			SCE_PL_STRING_QR,			FALSE },
	{ "string_qw",			SCE_PL_STRING_QW,			FALSE },
	{ "variable_indexer",	SCE_PL_VARIABLE_INDEXER,	FALSE },
	{ "punctuation",		SCE_PL_PUNCTUATION,			FALSE },
	{ "longquote",			SCE_PL_LONGQUOTE,			FALSE },
	{ "sub_prototype",		SCE_PL_SUB_PROTOTYPE,		FALSE },
	{ "format_ident",		SCE_PL_FORMAT_IDENT,		FALSE },
	{ "format",				SCE_PL_FORMAT,				FALSE },
	{ "string_var",			SCE_PL_STRING_VAR,			FALSE },
	{ "xlat",				SCE_PL_XLAT,				FALSE },
	{ "regex_var",			SCE_PL_REGEX_VAR,			FALSE },
	{ "regsubst_var",		SCE_PL_REGSUBST_VAR,		FALSE },
	{ "backticks_var",		SCE_PL_BACKTICKS_VAR,		FALSE },
	{ "here_qq_var",		SCE_PL_HERE_QQ_VAR,			FALSE },
	{ "here_qx_var",		SCE_PL_HERE_QX_VAR,			FALSE },
	{ "string_qq_var",		SCE_PL_STRING_QQ_VAR,		FALSE },
	{ "string_qx_var",		SCE_PL_STRING_QX_VAR,		FALSE },
	{ "string_qr_var",		SCE_PL_STRING_QR_VAR,		FALSE }
};
static const HLKeyword highlighting_keywords_PERL[] =
{
	{ "primary", 0,	FALSE }
};
#define highlighting_properties_PERL	EMPTY_PROPERTIES


/* PHP */
#define highlighting_lexer_PHP			SCLEX_HTML
#define highlighting_styles_PHP			highlighting_styles_HTML
#define highlighting_keywords_PHP		highlighting_keywords_HTML
#define highlighting_properties_PHP		highlighting_properties_HTML


/* PO (gettext) */
#define highlighting_lexer_PO		SCLEX_PO
static const HLStyle highlighting_styles_PO[] =
{
	{ "default",			SCE_PO_DEFAULT,				FALSE },
	{ "comment",			SCE_PO_COMMENT,				FALSE },
	{ "programmer_comment",	SCE_PO_PROGRAMMER_COMMENT,	FALSE },
	{ "reference",			SCE_PO_REFERENCE,			FALSE },
	{ "flags",				SCE_PO_FLAGS,				FALSE },
	{ "fuzzy",				SCE_PO_FUZZY,				FALSE },
	{ "msgid",				SCE_PO_MSGID,				FALSE },
	{ "msgid_text",			SCE_PO_MSGID_TEXT,			FALSE },
	{ "msgid_text_eol",		SCE_PO_MSGID_TEXT_EOL,		FALSE },
	{ "msgstr",				SCE_PO_MSGSTR,				FALSE },
	{ "msgstr_text",		SCE_PO_MSGSTR_TEXT,			FALSE },
	{ "msgstr_text_eol",	SCE_PO_MSGSTR_TEXT_EOL,		FALSE },
	{ "msgctxt",			SCE_PO_MSGCTXT,				FALSE },
	{ "msgctxt_text",		SCE_PO_MSGCTXT_TEXT,		FALSE },
	{ "msgctxt_text_eol",	SCE_PO_MSGCTXT_TEXT_EOL,	FALSE },
	{ "error",				SCE_PO_ERROR,				FALSE }
};
#define highlighting_keywords_PO	EMPTY_KEYWORDS
#define highlighting_properties_PO	EMPTY_PROPERTIES


/* PowerShell */
#define highlighting_lexer_POWERSHELL		SCLEX_POWERSHELL
static const HLStyle highlighting_styles_POWERSHELL[] =
{
	{ "default",			SCE_POWERSHELL_DEFAULT,				FALSE },
	{ "comment",			SCE_POWERSHELL_COMMENT,				FALSE },
	{ "string",				SCE_POWERSHELL_STRING,				FALSE },
	{ "character",			SCE_POWERSHELL_CHARACTER,			FALSE },
	{ "number",				SCE_POWERSHELL_NUMBER,				FALSE },
	{ "variable",			SCE_POWERSHELL_VARIABLE,			FALSE },
	{ "operator",			SCE_POWERSHELL_OPERATOR,			FALSE },
	{ "identifier",			SCE_POWERSHELL_IDENTIFIER,			FALSE },
	{ "keyword",			SCE_POWERSHELL_KEYWORD,				FALSE },
	{ "cmdlet",				SCE_POWERSHELL_CMDLET,				FALSE },
	{ "alias",				SCE_POWERSHELL_ALIAS,				FALSE },
	{ "function",			SCE_POWERSHELL_FUNCTION,			FALSE },
	{ "user1",				SCE_POWERSHELL_USER1,				FALSE },
	{ "commentstream",		SCE_POWERSHELL_COMMENTSTREAM,		FALSE },
	{ "here_string",		SCE_POWERSHELL_HERE_STRING,			FALSE },
	{ "here_character",		SCE_POWERSHELL_HERE_CHARACTER,		FALSE },
	{ "commentdockeyword",	SCE_POWERSHELL_COMMENTDOCKEYWORD,	FALSE },
};
static const HLKeyword highlighting_keywords_POWERSHELL[] =
{
	{ "keywords",	0,	FALSE },
	{ "cmdlets",	1,	FALSE },
	{ "aliases",	2,	FALSE },
	{ "functions",	3,	FALSE },
	{ "user1",		4,	FALSE },
	{ "docComment",	5,	FALSE },
};
#define highlighting_properties_POWERSHELL	EMPTY_PROPERTIES


/* Python */
#define highlighting_lexer_PYTHON		SCLEX_PYTHON
static const HLStyle highlighting_styles_PYTHON[] =
{
	{ "default",		SCE_P_DEFAULT,			FALSE },
	{ "commentline",	SCE_P_COMMENTLINE,		FALSE },
	{ "number",			SCE_P_NUMBER,			FALSE },
	{ "string",			SCE_P_STRING,			FALSE },
	{ "character",		SCE_P_CHARACTER,		FALSE },
	{ "word",			SCE_P_WORD,				FALSE },
	{ "triple",			SCE_P_TRIPLE,			FALSE },
	{ "tripledouble",	SCE_P_TRIPLEDOUBLE,		FALSE },
	{ "classname",		SCE_P_CLASSNAME,		FALSE },
	{ "defname",		SCE_P_DEFNAME,			FALSE },
	{ "operator",		SCE_P_OPERATOR,			FALSE },
	{ "identifier",		SCE_P_IDENTIFIER,		FALSE },
	{ "commentblock",	SCE_P_COMMENTBLOCK,		FALSE },
	{ "stringeol",		SCE_P_STRINGEOL,		FALSE },
	{ "word2",			SCE_P_WORD2,			FALSE },
	{ "fstring",		SCE_P_FSTRING,			FALSE },
	{ "fcharacter",		SCE_P_FCHARACTER,		FALSE },
	{ "ftriple",		SCE_P_FTRIPLE,			FALSE },
	{ "ftripledouble",	SCE_P_FTRIPLEDOUBLE,	FALSE },
	{ "decorator",		SCE_P_DECORATOR,		FALSE }
};
static const HLKeyword highlighting_keywords_PYTHON[] =
{
	{ "primary",			0,	FALSE },
	{ "identifiers",		1,	FALSE }
};
#define highlighting_properties_PYTHON	EMPTY_PROPERTIES


/* R */
#define highlighting_lexer_R		SCLEX_R
static const HLStyle highlighting_styles_R[] =
{
	{ "default",		SCE_R_DEFAULT,		FALSE },
	{ "comment",		SCE_R_COMMENT,		FALSE },
	{ "kword",			SCE_R_KWORD,		FALSE },
	{ "operator",		SCE_R_OPERATOR,		FALSE },
	{ "basekword",		SCE_R_BASEKWORD,	FALSE },
	{ "otherkword",		SCE_R_OTHERKWORD,	FALSE },
	{ "number",			SCE_R_NUMBER,		FALSE },
	{ "string",			SCE_R_STRING,		FALSE },
	{ "string2",		SCE_R_STRING2,		FALSE },
	{ "identifier",		SCE_R_IDENTIFIER,	FALSE },
	{ "infix",			SCE_R_INFIX,		FALSE },
	{ "infixeol",		SCE_R_INFIXEOL,		FALSE }
};
static const HLKeyword highlighting_keywords_R[] =
{
	{ "primary",			0,	FALSE },
	{ "package",			1,	FALSE },
	{ "package_other",		2,	FALSE }
};
#define highlighting_properties_R	EMPTY_PROPERTIES


/* Ruby */
#define highlighting_lexer_RUBY			SCLEX_RUBY
static const HLStyle highlighting_styles_RUBY[] =
{
	{ "default",			SCE_RB_DEFAULT,				FALSE },
	{ "commentline",		SCE_RB_COMMENTLINE,			FALSE },
	{ "number",				SCE_RB_NUMBER,				FALSE },
	{ "string",				SCE_RB_STRING,				FALSE },
	{ "character",			SCE_RB_CHARACTER,			FALSE },
	{ "word",				SCE_RB_WORD,				FALSE },
	{ "global",				SCE_RB_GLOBAL,				FALSE },
	{ "symbol",				SCE_RB_SYMBOL,				FALSE },
	{ "classname",			SCE_RB_CLASSNAME,			FALSE },
	{ "defname",			SCE_RB_DEFNAME,				FALSE },
	{ "operator",			SCE_RB_OPERATOR,			FALSE },
	{ "identifier",			SCE_RB_IDENTIFIER,			FALSE },
	{ "modulename",			SCE_RB_MODULE_NAME,			FALSE },
	{ "backticks",			SCE_RB_BACKTICKS,			FALSE },
	{ "instancevar",		SCE_RB_INSTANCE_VAR,		FALSE },
	{ "classvar",			SCE_RB_CLASS_VAR,			FALSE },
	{ "datasection",		SCE_RB_DATASECTION,			FALSE },
	{ "heredelim",			SCE_RB_HERE_DELIM,			FALSE },
	{ "worddemoted",		SCE_RB_WORD_DEMOTED,		FALSE },
	{ "stdin",				SCE_RB_STDIN,				FALSE },
	{ "stdout",				SCE_RB_STDOUT,				FALSE },
	{ "stderr",				SCE_RB_STDERR,				FALSE },
	{ "regex",				SCE_RB_REGEX,				FALSE },
	{ "here_q",				SCE_RB_HERE_Q,				FALSE },
	{ "here_qq",			SCE_RB_HERE_QQ,				FALSE },
	{ "here_qx",			SCE_RB_HERE_QX,				FALSE },
	{ "string_q",			SCE_RB_STRING_Q,			FALSE },
	{ "string_qq",			SCE_RB_STRING_QQ,			FALSE },
	{ "string_qx",			SCE_RB_STRING_QX,			FALSE },
	{ "string_qr",			SCE_RB_STRING_QR,			FALSE },
	{ "string_qw",			SCE_RB_STRING_QW,			FALSE },
	{ "upper_bound",		SCE_RB_UPPER_BOUND,			FALSE },
	{ "error",				SCE_RB_ERROR,				FALSE },
	{ "pod",				SCE_RB_POD,					FALSE }
};
static const HLKeyword highlighting_keywords_RUBY[] =
{
	{ "primary", 0,	FALSE }
};
#define highlighting_properties_RUBY	EMPTY_PROPERTIES

/* Rust */
#define highlighting_lexer_RUST		SCLEX_RUST
static const HLStyle highlighting_styles_RUST[] =
{
	{ "default",				SCE_RUST_DEFAULT,				FALSE },
	{ "commentblock",			SCE_RUST_COMMENTBLOCK,			FALSE },
	{ "commentline",			SCE_RUST_COMMENTLINE,			FALSE },
	{ "commentblockdoc",		SCE_RUST_COMMENTBLOCKDOC,		FALSE },
	{ "commentlinedoc",			SCE_RUST_COMMENTLINEDOC,		FALSE },
	{ "number",					SCE_RUST_NUMBER,				FALSE },
	{ "word",					SCE_RUST_WORD,					FALSE },
	{ "word2",					SCE_RUST_WORD2,					FALSE },
	{ "word3",					SCE_RUST_WORD3,					FALSE },
	{ "word4",					SCE_RUST_WORD4,					FALSE },
	{ "word5",					SCE_RUST_WORD5,					FALSE },
	{ "word6",					SCE_RUST_WORD6,					FALSE },
	{ "word7",					SCE_RUST_WORD7,					FALSE },
	{ "string",					SCE_RUST_STRING,				FALSE },
	{ "stringraw",				SCE_RUST_STRINGR,				FALSE },
	{ "character",				SCE_RUST_CHARACTER,				FALSE },
	{ "operator",				SCE_RUST_OPERATOR,				FALSE },
	{ "identifier",				SCE_RUST_IDENTIFIER,			FALSE },
	{ "lifetime",				SCE_RUST_LIFETIME,				FALSE },
	{ "macro",					SCE_RUST_MACRO,					FALSE },
	{ "lexerror",				SCE_RUST_LEXERROR,				FALSE },
	{ "bytestring",				SCE_RUST_BYTESTRING,			FALSE },
	{ "bytestringr",			SCE_RUST_BYTESTRINGR,			FALSE },
	{ "bytecharacter",			SCE_RUST_BYTECHARACTER,			FALSE }
};
static const HLKeyword highlighting_keywords_RUST[] =
{
	{ "primary",		0,	FALSE },
	/* SCI_SETKEYWORDS = 1 - secondary + global tags file types */
	{ "secondary",		1,	TRUE },
	{ "tertiary",		2,	FALSE },
	/* SCI_SETKEYWORDS = 3 is for current session types - see document_highlight_tags() */
};
#define highlighting_properties_RUST		EMPTY_PROPERTIES

/* SH */
#define highlighting_lexer_SH		SCLEX_BASH
static const HLStyle highlighting_styles_SH[] =
{
	{ "default",		SCE_SH_DEFAULT,		FALSE },
	{ "commentline",	SCE_SH_COMMENTLINE,	FALSE },
	{ "number",			SCE_SH_NUMBER,		FALSE },
	{ "word",			SCE_SH_WORD,		FALSE },
	{ "string",			SCE_SH_STRING,		FALSE },
	{ "character",		SCE_SH_CHARACTER,	FALSE },
	{ "operator",		SCE_SH_OPERATOR,	FALSE },
	{ "identifier",		SCE_SH_IDENTIFIER,	FALSE },
	{ "backticks",		SCE_SH_BACKTICKS,	FALSE },
	{ "param",			SCE_SH_PARAM,		FALSE },
	{ "scalar",			SCE_SH_SCALAR,		FALSE },
	{ "error",			SCE_SH_ERROR,		FALSE },
	{ "here_delim",		SCE_SH_HERE_DELIM,	FALSE },
	{ "here_q",			SCE_SH_HERE_Q,		FALSE }
};
static const HLKeyword highlighting_keywords_SH[] =
{
	{ "primary", 0,	FALSE }
};
#define highlighting_properties_SH	EMPTY_PROPERTIES


/* SMALLTALK */
#define highlighting_lexer_SMALLTALK SCLEX_SMALLTALK
static const HLStyle highlighting_styles_SMALLTALK[] =
{
	{ "default",			SCE_ST_DEFAULT,		FALSE },
	{ "special",			SCE_ST_SPECIAL,		FALSE },
	{ "symbol",				SCE_ST_SYMBOL,		FALSE },
	{ "assignment",			SCE_ST_ASSIGN,		FALSE },
	{ "return",				SCE_ST_RETURN,		FALSE },
	{ "number",				SCE_ST_NUMBER,		FALSE },
	{ "binary",				SCE_ST_BINARY,		FALSE },
	{ "special_selector",	SCE_ST_SPEC_SEL,	FALSE },
	{ "keyword_send",		SCE_ST_KWSEND,		FALSE },
	{ "global",				SCE_ST_GLOBAL,		FALSE },
	{ "self",				SCE_ST_SELF,		FALSE },
	{ "super",				SCE_ST_SUPER,		FALSE },
	{ "nil",				SCE_ST_NIL,			FALSE },
	{ "bool",				SCE_ST_BOOL,		FALSE },
	{ "comment",			SCE_ST_COMMENT,		FALSE },
	{ "string",				SCE_ST_STRING,		FALSE },
	{ "character",			SCE_ST_CHARACTER,	FALSE }
};
static const HLKeyword highlighting_keywords_SMALLTALK[] =
{
	{ "special_selector", 0,	FALSE }
};
#define highlighting_properties_SMALLTALK	EMPTY_PROPERTIES


/* SQL */
#define highlighting_lexer_SQL			SCLEX_SQL
static const HLStyle highlighting_styles_SQL[] =
{
	{ "default",				SCE_SQL_DEFAULT,					FALSE },
	{ "comment",				SCE_SQL_COMMENT,					FALSE },
	{ "commentline",			SCE_SQL_COMMENTLINE,				FALSE },
	{ "commentdoc",				SCE_SQL_COMMENTDOC,					FALSE },
	{ "commentlinedoc",			SCE_SQL_COMMENTLINEDOC,				FALSE },
	{ "commentdockeyword",		SCE_SQL_COMMENTDOCKEYWORD,			FALSE },
	{ "commentdockeyworderror",	SCE_SQL_COMMENTDOCKEYWORDERROR,		FALSE },
	{ "number",					SCE_SQL_NUMBER,						FALSE },
	{ "word",					SCE_SQL_WORD,						FALSE },
	{ "word2",					SCE_SQL_WORD2,						FALSE },
	{ "string",					SCE_SQL_STRING,						FALSE },
	{ "character",				SCE_SQL_CHARACTER,					FALSE },
	{ "operator",				SCE_SQL_OPERATOR,					FALSE },
	{ "identifier",				SCE_SQL_IDENTIFIER,					FALSE },
	{ "sqlplus",				SCE_SQL_SQLPLUS,					FALSE },
	{ "sqlplus_prompt",			SCE_SQL_SQLPLUS_PROMPT,				FALSE },
	{ "sqlplus_comment",		SCE_SQL_SQLPLUS_COMMENT,			FALSE },
	{ "quotedidentifier",		SCE_SQL_QUOTEDIDENTIFIER,			FALSE },
	{ "qoperator",				SCE_SQL_QOPERATOR,					FALSE }
	/* these are for user-defined keywords we don't set yet */
	/*{ "user1",				SCE_SQL_USER1,						FALSE },
	{ "user2",					SCE_SQL_USER2,						FALSE },
	{ "user3",					SCE_SQL_USER3,						FALSE },
	{ "user4",					SCE_SQL_USER4,						FALSE }*/
};
static const HLKeyword highlighting_keywords_SQL[] =
{
	{ "keywords", 0,	FALSE }
};
#define highlighting_properties_SQL		EMPTY_PROPERTIES


/* TCL */
#define highlighting_lexer_TCL			SCLEX_TCL
static const HLStyle highlighting_styles_TCL[] =
{
	{ "default",			SCE_TCL_DEFAULT,		FALSE },
	{ "comment",			SCE_TCL_COMMENT,		FALSE },
	{ "commentline",		SCE_TCL_COMMENTLINE,	FALSE },
	{ "number",				SCE_TCL_NUMBER,			FALSE },
	{ "operator",			SCE_TCL_OPERATOR,		FALSE },
	{ "identifier",			SCE_TCL_IDENTIFIER,		FALSE },
	{ "wordinquote",		SCE_TCL_WORD_IN_QUOTE,	FALSE },
	{ "inquote",			SCE_TCL_IN_QUOTE,		FALSE },
	{ "substitution",		SCE_TCL_SUBSTITUTION,	FALSE },
	{ "modifier",			SCE_TCL_MODIFIER,		FALSE },
	{ "expand",				SCE_TCL_EXPAND,			FALSE },
	{ "wordtcl",			SCE_TCL_WORD,			FALSE },
	{ "wordtk",				SCE_TCL_WORD2,			FALSE },
	{ "worditcl",			SCE_TCL_WORD3,			FALSE },
	{ "wordtkcmds",			SCE_TCL_WORD4,			FALSE },
	{ "wordexpand",			SCE_TCL_WORD5,			FALSE },
	{ "commentbox",			SCE_TCL_COMMENT_BOX,	FALSE },
	{ "blockcomment",		SCE_TCL_BLOCK_COMMENT,	FALSE },
	{ "subbrace",			SCE_TCL_SUB_BRACE,		FALSE }
	/* these are for user-defined keywords we don't set yet */
	/*{ "user2",			SCE_TCL_WORD6,			FALSE },
	{ "user3",				SCE_TCL_WORD7,			FALSE },
	{ "user4",				SCE_TCL_WORD8,			FALSE }*/
};
static const HLKeyword highlighting_keywords_TCL[] =
{
	{ "tcl",		0,	FALSE },
	{ "tk",			1,	FALSE },
	{ "itcl",		2,	FALSE },
	{ "tkcommands", 3,	FALSE },
	{ "expand",		4,	FALSE }
};
#define highlighting_properties_TCL		EMPTY_PROPERTIES


/* Txt2Tags */
#define highlighting_lexer_TXT2TAGS			SCLEX_TXT2TAGS
static const HLStyle highlighting_styles_TXT2TAGS[] =
{
	{ "default",		SCE_TXT2TAGS_DEFAULT,		FALSE },
	{ "default",		SCE_TXT2TAGS_LINE_BEGIN,	FALSE }, /* FIXME: remappings should be avoided */
	{ "default",		SCE_TXT2TAGS_PRECHAR,		FALSE },
	{ "strong",			SCE_TXT2TAGS_STRONG1,		FALSE },
	{ "strong",			SCE_TXT2TAGS_STRONG2,		FALSE },
	{ "emphasis",		SCE_TXT2TAGS_EM1,			FALSE },
	{ "underlined",		SCE_TXT2TAGS_EM2,			FALSE }, /* WTF? */
	{ "header1",		SCE_TXT2TAGS_HEADER1,		FALSE },
	{ "header2",		SCE_TXT2TAGS_HEADER2,		FALSE },
	{ "header3",		SCE_TXT2TAGS_HEADER3,		FALSE },
	{ "header4",		SCE_TXT2TAGS_HEADER4,		FALSE },
	{ "header5",		SCE_TXT2TAGS_HEADER5,		FALSE },
	{ "header6",		SCE_TXT2TAGS_HEADER6,		FALSE },
	{ "ulist_item",		SCE_TXT2TAGS_ULIST_ITEM,	FALSE },
	{ "olist_item",		SCE_TXT2TAGS_OLIST_ITEM,	FALSE },
	{ "blockquote",		SCE_TXT2TAGS_BLOCKQUOTE,	FALSE },
	{ "strikeout",		SCE_TXT2TAGS_STRIKEOUT,		FALSE },
	{ "hrule",			SCE_TXT2TAGS_HRULE,			FALSE },
	{ "link",			SCE_TXT2TAGS_LINK,			FALSE },
	{ "code",			SCE_TXT2TAGS_CODE,			FALSE },
	{ "code",			SCE_TXT2TAGS_CODE2,			FALSE },
	{ "codebk",			SCE_TXT2TAGS_CODEBK,		FALSE },
	{ "comment",		SCE_TXT2TAGS_COMMENT,		FALSE },
	{ "option",			SCE_TXT2TAGS_OPTION,		FALSE },
	{ "preproc",		SCE_TXT2TAGS_PREPROC,		FALSE },
	{ "postproc",		SCE_TXT2TAGS_POSTPROC,		FALSE }
};
#define highlighting_keywords_TXT2TAGS		EMPTY_KEYWORDS
#define highlighting_properties_TXT2TAGS	EMPTY_PROPERTIES


/* VHDL */
#define highlighting_lexer_VHDL			SCLEX_VHDL
static const HLStyle highlighting_styles_VHDL[] =
{
	{ "default",			SCE_VHDL_DEFAULT,			FALSE },
	{ "comment",			SCE_VHDL_COMMENT,			FALSE },
	{ "comment_line_bang",	SCE_VHDL_COMMENTLINEBANG,	FALSE },
	{ "block_comment",		SCE_VHDL_BLOCK_COMMENT,		FALSE },
	{ "number",				SCE_VHDL_NUMBER,			FALSE },
	{ "string",				SCE_VHDL_STRING,			FALSE },
	{ "operator",			SCE_VHDL_OPERATOR,			FALSE },
	{ "identifier",			SCE_VHDL_IDENTIFIER,		FALSE },
	{ "stringeol",			SCE_VHDL_STRINGEOL,			FALSE },
	{ "keyword",			SCE_VHDL_KEYWORD,			FALSE },
	{ "stdoperator",		SCE_VHDL_STDOPERATOR,		FALSE },
	{ "attribute",			SCE_VHDL_ATTRIBUTE,			FALSE },
	{ "stdfunction",		SCE_VHDL_STDFUNCTION,		FALSE },
	{ "stdpackage",			SCE_VHDL_STDPACKAGE,		FALSE },
	{ "stdtype",			SCE_VHDL_STDTYPE,			FALSE },
	{ "userword",			SCE_VHDL_USERWORD,			FALSE }
};
static const HLKeyword highlighting_keywords_VHDL[] =
{
	{ "keywords",		0,	FALSE },
	{ "operators",		1,	FALSE },
	{ "attributes",		2,	FALSE },
	{ "std_functions",	3,	FALSE },
	{ "std_packages",	4,	FALSE },
	{ "std_types",		5,	FALSE },
	{ "userwords",		6,	FALSE },
};
#define highlighting_properties_VHDL	EMPTY_PROPERTIES


/* Verilog */
#define highlighting_lexer_VERILOG			SCLEX_VERILOG
static const HLStyle highlighting_styles_VERILOG[] =
{
	{ "default",			SCE_V_DEFAULT,			FALSE },
	{ "comment",			SCE_V_COMMENT,			FALSE },
	{ "comment_line",		SCE_V_COMMENTLINE,		FALSE },
	{ "comment_line_bang",	SCE_V_COMMENTLINEBANG,	FALSE },
	{ "number",				SCE_V_NUMBER,			FALSE },
	{ "word",				SCE_V_WORD,				FALSE },
	{ "string",				SCE_V_STRING,			FALSE },
	{ "word2",				SCE_V_WORD2,			FALSE },
	{ "word3",				SCE_V_WORD3,			FALSE },
	{ "preprocessor",		SCE_V_PREPROCESSOR,		FALSE },
	{ "operator",			SCE_V_OPERATOR,			FALSE },
	{ "identifier",			SCE_V_IDENTIFIER,		FALSE },
	{ "stringeol",			SCE_V_STRINGEOL,		FALSE },
	{ "userword",			SCE_V_USER,				FALSE },
	{ "comment_word",		SCE_V_COMMENT_WORD,		FALSE },
	{ "input",				SCE_V_INPUT,			FALSE },
	{ "output",				SCE_V_OUTPUT,			FALSE },
	{ "inout",				SCE_V_INOUT,			FALSE },
	{ "port_connect",		SCE_V_PORT_CONNECT,		FALSE }
};
static const HLKeyword highlighting_keywords_VERILOG[] =
{
	{ "word",		0,	FALSE },
	{ "word2",		1,	FALSE },
	{ "word3",		2,	FALSE },
	{ "docComment", 4,	FALSE }
};
#define highlighting_properties_VERILOG		EMPTY_PROPERTIES


/* XML */
#define highlighting_lexer_XML			SCLEX_XML
#define highlighting_styles_XML			highlighting_styles_HTML
static const HLKeyword highlighting_keywords_XML[] =
{
	{ "sgml", 5,	FALSE }
};
#define highlighting_properties_XML		highlighting_properties_HTML


/* YAML */
#define highlighting_lexer_YAML			SCLEX_YAML
static const HLStyle highlighting_styles_YAML[] =
{
	{ "default",	SCE_YAML_DEFAULT,		FALSE },
	{ "comment",	SCE_YAML_COMMENT,		FALSE },
	{ "identifier",	SCE_YAML_IDENTIFIER,	FALSE },
	{ "keyword",	SCE_YAML_KEYWORD,		FALSE },
	{ "number",		SCE_YAML_NUMBER,		FALSE },
	{ "reference",	SCE_YAML_REFERENCE,		FALSE },
	{ "document",	SCE_YAML_DOCUMENT,		FALSE },
	{ "text",		SCE_YAML_TEXT,			FALSE },
	{ "error",		SCE_YAML_ERROR,			FALSE },
	{ "operator",	SCE_YAML_OPERATOR,		FALSE }
};
static const HLKeyword highlighting_keywords_YAML[] =
{
	{ "keywords", 0,	FALSE }
};
#define highlighting_properties_YAML	EMPTY_PROPERTIES


/* Zephir */
#define highlighting_lexer_ZEPHIR		SCLEX_PHPSCRIPT
#define highlighting_styles_ZEPHIR		highlighting_styles_PHP
#define highlighting_keywords_ZEPHIR	highlighting_keywords_PHP
#define highlighting_properties_ZEPHIR	highlighting_properties_PHP

G_END_DECLS

#endif /* GEANY_HIGHLIGHTING_MAPPINGS_H */
