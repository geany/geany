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
 * 									item is also used for the default style.
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
	guint		 style;			/* SCI style */
	const gchar	*name;			/* style name in the filetypes.* file */
	gboolean	 fill_eol;		/* whether to set EOLFILLED flag to this style */
	gboolean	 sub_stylable;	/* Whether substyles can be allocated for this style */
} HLStyle;

typedef struct
{
	guint		 id;	/* SCI keyword ID */
	const gchar	*key;	/* keywords entry name in the filetypes.* file */
	gboolean	 merge;	/* whether to merge with session types */
} HLKeyword;

typedef struct
{
	const gchar *property;
	const gchar *value;
} HLProperty;


#define EMPTY_KEYWORDS		((HLKeyword *) NULL)
#define EMPTY_PROPERTIES	((HLProperty *) NULL)

/* like G_N_ELEMENTS() but supports @array being NULL (for empty entries).
 * The straightforward `((array != NULL) ? G_N_ELEMENTS(array) : 0)` is not
 * used here because of GCC8's -Wsizeof-pointer-div which doesn't realize the
 * result of G_N_ELEMENTS() is never actually used when `array` is NULL.
 * This implementation gives the same result as the LHS of the division
 * becomes 0 when `array` is NULL, but is not a case that GCC can misinterpret
 * and warn about.
 * An alternative solution would be using zero-sized arrays instead of NULLs,
 * but zero-sized arrays are forbidden by ISO C */
#define HL_N_ENTRIES(array) ((sizeof(array) * ((array) != NULL)) / sizeof((array)[0]))


/* Abaqus */
#define highlighting_lexer_ABAQUS			SCLEX_ABAQUS
static const HLStyle highlighting_styles_ABAQUS[] =
{
	{ SCE_ABAQUS_DEFAULT,		"default",		FALSE,	FALSE },
	{ SCE_ABAQUS_COMMENT,		"comment",		FALSE,	FALSE },
	{ SCE_ABAQUS_NUMBER,		"number",		FALSE,	FALSE },
	{ SCE_ABAQUS_STRING,		"string",		FALSE,	FALSE },
	{ SCE_ABAQUS_OPERATOR,		"operator",		FALSE,	FALSE },
	{ SCE_ABAQUS_PROCESSOR,		"processor",	FALSE,	FALSE },
	{ SCE_ABAQUS_STARCOMMAND,	"starcommand",	FALSE,	FALSE },
	{ SCE_ABAQUS_ARGUMENT,		"argument",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_ABAQUS[] =
{
	{ 0, "processors", FALSE },
	{ 1, "commands", FALSE },
	{ 2, "slashommands", FALSE },
	{ 3, "starcommands", FALSE },
	{ 4, "arguments", FALSE },
	{ 5, "functions", FALSE }
};
#define highlighting_properties_ABAQUS		EMPTY_PROPERTIES


/* Ada */
#define highlighting_lexer_ADA			SCLEX_ADA
static const HLStyle highlighting_styles_ADA[] =
{
	{ SCE_ADA_DEFAULT,		"default",		FALSE,	FALSE },
	{ SCE_ADA_WORD,			"word",			FALSE,	FALSE },
	{ SCE_ADA_IDENTIFIER,	"identifier",	FALSE,	FALSE },
	{ SCE_ADA_NUMBER,		"number",		FALSE,	FALSE },
	{ SCE_ADA_DELIMITER,	"delimiter",	FALSE,	FALSE },
	{ SCE_ADA_CHARACTER,	"character",	FALSE,	FALSE },
	{ SCE_ADA_CHARACTEREOL,	"charactereol",	FALSE,	FALSE },
	{ SCE_ADA_STRING,		"string",		FALSE,	FALSE },
	{ SCE_ADA_STRINGEOL,	"stringeol",	FALSE,	FALSE },
	{ SCE_ADA_LABEL,		"label",		FALSE,	FALSE },
	{ SCE_ADA_COMMENTLINE,	"commentline",	FALSE,	FALSE },
	{ SCE_ADA_ILLEGAL,		"illegal",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_ADA[] =
{
	{ 0, "primary",	FALSE }
};
#define highlighting_properties_ADA		EMPTY_PROPERTIES


/* ActionScript */
#define highlighting_lexer_AS		SCLEX_CPP
#define highlighting_styles_AS		highlighting_styles_C
static const HLKeyword highlighting_keywords_AS[] =
{
	{ 0, "primary",		FALSE },
	{ 1, "secondary",	FALSE },
	{ 3, "classes",		FALSE }
};
#define highlighting_properties_AS	highlighting_properties_C


/* Asccidoc */
#define highlighting_lexer_ASCIIDOC			SCLEX_ASCIIDOC
static const HLStyle highlighting_styles_ASCIIDOC[] =
{
	{ SCE_ASCIIDOC_DEFAULT,		"default",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_STRONG1,		"strong",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_STRONG2,		"strong",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_EM1,			"emphasis",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_EM2,			"emphasis",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_HEADER1,		"header1",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_HEADER2,		"header2",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_HEADER3,		"header3",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_HEADER4,		"header4",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_HEADER5,		"header5",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_HEADER6,		"header6",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_ULIST_ITEM,	"ulist_item",	FALSE,	FALSE },
	{ SCE_ASCIIDOC_OLIST_ITEM,	"olist_item",	FALSE,	FALSE },
	{ SCE_ASCIIDOC_BLOCKQUOTE,	"blockquote",	FALSE,	FALSE },
	{ SCE_ASCIIDOC_LINK,		"link",			FALSE,	FALSE },
	{ SCE_ASCIIDOC_CODEBK,		"code",			FALSE,	FALSE },
	{ SCE_ASCIIDOC_PASSBK,		"passthrough",	FALSE,	FALSE },
	{ SCE_ASCIIDOC_COMMENT,		"comment",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_COMMENTBK,	"comment",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_LITERAL,		"literal",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_LITERALBK,	"literal",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_ATTRIB,		"attrib",		FALSE,	FALSE },
	{ SCE_ASCIIDOC_ATTRIBVAL,	"attribval",	FALSE,	FALSE },
	{ SCE_ASCIIDOC_MACRO,		"macro",		FALSE,	FALSE }
};
#define highlighting_keywords_ASCIIDOC		EMPTY_KEYWORDS
#define highlighting_properties_ASCIIDOC	EMPTY_PROPERTIES


/* ASM */
#define highlighting_lexer_ASM			SCLEX_ASM
static const HLStyle highlighting_styles_ASM[] =
{
	{ SCE_ASM_DEFAULT,			"default",				FALSE,	FALSE },
	{ SCE_ASM_COMMENT,			"comment",				FALSE,	FALSE },
	{ SCE_ASM_NUMBER,			"number",				FALSE,	FALSE },
	{ SCE_ASM_STRING,			"string",				FALSE,	FALSE },
	{ SCE_ASM_OPERATOR,			"operator",				FALSE,	FALSE },
	{ SCE_ASM_IDENTIFIER,		"identifier",			FALSE,	FALSE },
	{ SCE_ASM_CPUINSTRUCTION,	"cpuinstruction",		FALSE,	FALSE },
	{ SCE_ASM_MATHINSTRUCTION,	"mathinstruction",		FALSE,	FALSE },
	{ SCE_ASM_REGISTER,			"register",				FALSE,	FALSE },
	{ SCE_ASM_DIRECTIVE,		"directive",			FALSE,	FALSE },
	{ SCE_ASM_DIRECTIVEOPERAND,	"directiveoperand",		FALSE,	FALSE },
	{ SCE_ASM_COMMENTBLOCK,		"commentblock",			FALSE,	FALSE },
	{ SCE_ASM_CHARACTER,		"character",			FALSE,	FALSE },
	{ SCE_ASM_STRINGEOL,		"stringeol",			FALSE,	FALSE },
	{ SCE_ASM_EXTINSTRUCTION,	"extinstruction",		FALSE,	FALSE },
	{ SCE_ASM_COMMENTDIRECTIVE,	"commentdirective",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_ASM[] =
{
	{ 0, "instructions",	FALSE },
	/*{ 1, "instructions",	FALSE },*/
	{ 2, "registers",		FALSE },
	{ 3, "directives",		FALSE }
	/*{ 5, "instructions",	FALSE }*/
};
#define highlighting_properties_ASM		EMPTY_PROPERTIES


/* BASIC */
#define highlighting_lexer_BASIC		SCLEX_FREEBASIC
static const HLStyle highlighting_styles_BASIC[] =
{
	{ SCE_B_DEFAULT,		"default",			FALSE,	FALSE },
	{ SCE_B_COMMENT,		"comment",			FALSE,	FALSE },
	{ SCE_B_COMMENTBLOCK,	"commentblock",		FALSE,	FALSE },
	{ SCE_B_DOCLINE,		"docline",			FALSE,	FALSE },
	{ SCE_B_DOCBLOCK,		"docblock",			FALSE,	FALSE },
	{ SCE_B_DOCKEYWORD,		"dockeyword",		FALSE,	FALSE },
	{ SCE_B_NUMBER,			"number",			FALSE,	FALSE },
	{ SCE_B_KEYWORD,		"word",				FALSE,	FALSE },
	{ SCE_B_STRING,			"string",			FALSE,	FALSE },
	{ SCE_B_PREPROCESSOR,	"preprocessor",		FALSE,	FALSE },
	{ SCE_B_OPERATOR,		"operator",			FALSE,	FALSE },
	{ SCE_B_IDENTIFIER,		"identifier",		FALSE,	FALSE },
	{ SCE_B_DATE,			"date",				FALSE,	FALSE },
	{ SCE_B_STRINGEOL,		"stringeol",		FALSE,	FALSE },
	{ SCE_B_KEYWORD2,		"word2",			FALSE,	FALSE },
	{ SCE_B_KEYWORD3,		"word3",			FALSE,	FALSE },
	{ SCE_B_KEYWORD4,		"word4",			FALSE,	FALSE },
	{ SCE_B_CONSTANT,		"constant",			FALSE,	FALSE },
	{ SCE_B_ASM,			"asm",				FALSE,	FALSE },
	{ SCE_B_LABEL,			"label",			FALSE,	FALSE },
	{ SCE_B_ERROR,			"error",			FALSE,	FALSE },
	{ SCE_B_HEXNUMBER,		"hexnumber",		FALSE,	FALSE },
	{ SCE_B_BINNUMBER,		"binnumber",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_BASIC[] =
{
	{ 0, "keywords",		FALSE },
	{ 1, "preprocessor",	FALSE },
	{ 2, "user1",			FALSE },
	{ 3, "user2",			FALSE }
};
#define highlighting_properties_BASIC	EMPTY_PROPERTIES


/* BATCH */
#define highlighting_lexer_BATCH		SCLEX_BATCH
static const HLStyle highlighting_styles_BATCH[] =
{
	{ SCE_BAT_DEFAULT,		"default",		FALSE,	FALSE },
	{ SCE_BAT_COMMENT,		"comment",		FALSE,	FALSE },
	{ SCE_BAT_LABEL,		"label",		FALSE,	FALSE },
	{ SCE_BAT_WORD,			"word",			FALSE,	FALSE },
	{ SCE_BAT_HIDE,			"hide",			FALSE,	FALSE },
	{ SCE_BAT_COMMAND,		"command",		FALSE,	FALSE },
	{ SCE_BAT_IDENTIFIER,	"identifier",	FALSE,	FALSE },
	{ SCE_BAT_OPERATOR,		"operator",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_BATCH[] =
{
	{ 0, "keywords",			FALSE },
	{ 1, "keywords_optional",	FALSE }
};
#define highlighting_properties_BATCH	EMPTY_PROPERTIES


/* C */
/* Also used by some other SCLEX_CPP-based filetypes */
#define highlighting_lexer_C		SCLEX_CPP
static const HLStyle highlighting_styles_C[] =
{
	{ SCE_C_DEFAULT,				"default",					FALSE,	FALSE },
	{ SCE_C_COMMENT,				"comment",					FALSE,	FALSE },
	{ SCE_C_COMMENTLINE,			"commentline",				FALSE,	FALSE },
	{ SCE_C_COMMENTDOC,				"commentdoc",				FALSE,	FALSE },
	{ SCE_C_PREPROCESSORCOMMENT,	"preprocessorcomment",		FALSE,	FALSE },
	{ SCE_C_PREPROCESSORCOMMENTDOC,	"preprocessorcommentdoc",	FALSE,	FALSE },
	{ SCE_C_NUMBER,					"number",					FALSE,	FALSE },
	{ SCE_C_WORD,					"word",						FALSE,	FALSE },
	{ SCE_C_WORD2,					"word2",					FALSE,	FALSE },
	{ SCE_C_STRING,					"string",					FALSE,	FALSE },
	{ SCE_C_STRINGRAW,				"stringraw",				FALSE,	FALSE },
	{ SCE_C_CHARACTER,				"character",				FALSE,	FALSE },
	{ SCE_C_USERLITERAL,			"userliteral",				FALSE,	FALSE },
	{ SCE_C_UUID,					"uuid",						FALSE,	FALSE },
	{ SCE_C_PREPROCESSOR,			"preprocessor",				FALSE,	FALSE },
	{ SCE_C_OPERATOR,				"operator",					FALSE,	FALSE },
	{ SCE_C_IDENTIFIER,				"identifier",				FALSE,	TRUE },
	{ SCE_C_STRINGEOL,				"stringeol",				FALSE,	FALSE },
	{ SCE_C_VERBATIM,				"verbatim",					FALSE,	FALSE },
	/* triple verbatims use the same style */
	{ SCE_C_TRIPLEVERBATIM,			"verbatim",					FALSE,	FALSE },
	{ SCE_C_REGEX,					"regex",					FALSE,	FALSE },
	{ SCE_C_HASHQUOTEDSTRING,		"hashquotedstring",			FALSE,	FALSE },
	{ SCE_C_COMMENTLINEDOC,			"commentlinedoc",			FALSE,	FALSE },
	{ SCE_C_COMMENTDOCKEYWORD,		"commentdockeyword",		FALSE,	TRUE },
	{ SCE_C_COMMENTDOCKEYWORDERROR,	"commentdockeyworderror",	FALSE,	FALSE },
	/* used for local structs and typedefs */
	{ SCE_C_GLOBALCLASS,			"globalclass",				FALSE,	FALSE },
	{ SCE_C_TASKMARKER,				"taskmarker",				FALSE,	FALSE },
	{ SCE_C_ESCAPESEQUENCE,			"escapesequence",			FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_C[] =
{
	{ 0, "primary",		FALSE },
	/* SCI_SETKEYWORDS = 1 - secondary + global tags file types, see below */
	{ 1, "secondary",	TRUE },
	{ 2, "docComment",	FALSE }
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
	{ SCE_CAML_DEFAULT,		"default",		FALSE,	FALSE },
	{ SCE_CAML_COMMENT,		"comment",		FALSE,	FALSE },
	{ SCE_CAML_COMMENT1,	"comment1",		FALSE,	FALSE },
	{ SCE_CAML_COMMENT2,	"comment2",		FALSE,	FALSE },
	{ SCE_CAML_COMMENT3,	"comment3",		FALSE,	FALSE },
	{ SCE_CAML_NUMBER,		"number",		FALSE,	FALSE },
	{ SCE_CAML_KEYWORD,		"keyword",		FALSE,	FALSE },
	{ SCE_CAML_KEYWORD2,	"keyword2",		FALSE,	FALSE },
	{ SCE_CAML_KEYWORD3,	"keyword3",		FALSE,	FALSE },
	{ SCE_CAML_STRING,		"string",		FALSE,	FALSE },
	{ SCE_CAML_CHAR,		"char",			FALSE,	FALSE },
	{ SCE_CAML_OPERATOR,	"operator",		FALSE,	FALSE },
	{ SCE_CAML_IDENTIFIER,	"identifier",	FALSE,	FALSE },
	{ SCE_CAML_TAGNAME,		"tagname",		FALSE,	FALSE },
	{ SCE_CAML_LINENUM,		"linenum",		FALSE,	FALSE },
	{ SCE_CAML_WHITE,		"white",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_CAML[] =
{
	{ 0, "keywords",			FALSE },
	{ 1, "keywords_optional",	FALSE }
};
#define highlighting_properties_CAML	EMPTY_PROPERTIES


/* CMake */
#define highlighting_lexer_CMAKE		SCLEX_CMAKE
static const HLStyle highlighting_styles_CMAKE[] =
{
	{ SCE_CMAKE_DEFAULT,		"default",		FALSE,	FALSE },
	{ SCE_CMAKE_COMMENT,		"comment",		FALSE,	FALSE },
	{ SCE_CMAKE_STRINGDQ,		"stringdq",		FALSE,	FALSE },
	{ SCE_CMAKE_STRINGLQ,		"stringlq",		FALSE,	FALSE },
	{ SCE_CMAKE_STRINGRQ,		"stringrq",		FALSE,	FALSE },
	{ SCE_CMAKE_COMMANDS,		"command",		FALSE,	FALSE },
	{ SCE_CMAKE_PARAMETERS,		"parameters",	FALSE,	FALSE },
	{ SCE_CMAKE_VARIABLE,		"variable",		FALSE,	FALSE },
	{ SCE_CMAKE_USERDEFINED,	"userdefined",	FALSE,	FALSE },
	{ SCE_CMAKE_WHILEDEF,		"whiledef",		FALSE,	FALSE },
	{ SCE_CMAKE_FOREACHDEF,		"foreachdef",	FALSE,	FALSE },
	{ SCE_CMAKE_IFDEFINEDEF,	"ifdefinedef",	FALSE,	FALSE },
	{ SCE_CMAKE_MACRODEF,		"macrodef",		FALSE,	FALSE },
	{ SCE_CMAKE_STRINGVAR,		"stringvar",	FALSE,	FALSE },
	{ SCE_CMAKE_NUMBER,			"number",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_CMAKE[] =
{
	{ 0, "commands",	FALSE },
	{ 1, "parameters",	FALSE },
	{ 2, "userdefined",	FALSE }
};
#define highlighting_properties_CMAKE	EMPTY_PROPERTIES

/* CoffeeScript */
#define highlighting_lexer_COFFEESCRIPT		SCLEX_COFFEESCRIPT
static const HLStyle highlighting_styles_COFFEESCRIPT[] =
{
	{ SCE_COFFEESCRIPT_DEFAULT,					"default",				FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_COMMENTLINE,				"commentline",			FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_NUMBER,					"number",				FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_WORD,					"word",					FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_STRING,					"string",				FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_CHARACTER,				"character",			FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_OPERATOR,				"operator",				FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_IDENTIFIER,				"identifier",			FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_STRINGEOL,				"stringeol",			FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_REGEX,					"regex",				FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_WORD2,					"word2",				FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_GLOBALCLASS,				"globalclass",			FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_COMMENTBLOCK,			"commentblock",			FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_VERBOSE_REGEX,			"verbose_regex",		FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_VERBOSE_REGEX_COMMENT,	"verbose_regex_comment",FALSE,	FALSE },
	{ SCE_COFFEESCRIPT_INSTANCEPROPERTY,		"instanceproperty",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_COFFEESCRIPT[] =
{
	{ 0, "primary",		FALSE },
	{ 1, "secondary",	FALSE },
	{ 3, "globalclass",	FALSE }
};
#define highlighting_properties_COFFEESCRIPT	EMPTY_PROPERTIES


/* CSS */
#define highlighting_lexer_CSS			SCLEX_CSS
static const HLStyle highlighting_styles_CSS[] =
{
	{ SCE_CSS_DEFAULT,					"default",					FALSE,	FALSE },
	{ SCE_CSS_COMMENT,					"comment",					FALSE,	FALSE },
	{ SCE_CSS_TAG,						"tag",						FALSE,	FALSE },
	{ SCE_CSS_CLASS,					"class",					FALSE,	FALSE },
	{ SCE_CSS_PSEUDOCLASS,				"pseudoclass",				FALSE,	FALSE },
	{ SCE_CSS_UNKNOWN_PSEUDOCLASS,		"unknown_pseudoclass",		FALSE,	FALSE },
	{ SCE_CSS_UNKNOWN_IDENTIFIER,		"unknown_identifier",		FALSE,	FALSE },
	{ SCE_CSS_OPERATOR,					"operator",					FALSE,	FALSE },
	{ SCE_CSS_IDENTIFIER,				"identifier",				FALSE,	FALSE },
	{ SCE_CSS_DOUBLESTRING,				"doublestring",				FALSE,	FALSE },
	{ SCE_CSS_SINGLESTRING,				"singlestring",				FALSE,	FALSE },
	{ SCE_CSS_ATTRIBUTE,				"attribute",				FALSE,	FALSE },
	{ SCE_CSS_VALUE,					"value",					FALSE,	FALSE },
	{ SCE_CSS_ID,						"id",						FALSE,	FALSE },
	{ SCE_CSS_IDENTIFIER2,				"identifier2",				FALSE,	FALSE },
	{ SCE_CSS_VARIABLE,					"variable",					FALSE,	FALSE },
	{ SCE_CSS_IMPORTANT,				"important",				FALSE,	FALSE },
	{ SCE_CSS_DIRECTIVE,				"directive",				FALSE,	FALSE },
	{ SCE_CSS_IDENTIFIER3,				"identifier3",				FALSE,	FALSE },
	{ SCE_CSS_PSEUDOELEMENT,			"pseudoelement",			FALSE,	FALSE },
	{ SCE_CSS_EXTENDED_IDENTIFIER,		"extended_identifier",		FALSE,	FALSE },
	{ SCE_CSS_EXTENDED_PSEUDOCLASS,		"extended_pseudoclass",		FALSE,	FALSE },
	{ SCE_CSS_EXTENDED_PSEUDOELEMENT,	"extended_pseudoelement",	FALSE,	FALSE },
	{ SCE_CSS_GROUP_RULE,				"group_rule",				FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_CSS[] =
{
	{ 0, "primary",					FALSE },
	{ 1, "pseudoclasses",			FALSE },
	{ 2, "secondary",				FALSE },
	{ 3, "css3_properties",			FALSE },
	{ 4, "pseudo_elements",			FALSE },
	{ 5, "browser_css_properties",	FALSE },
	{ 6, "browser_pseudo_classes",	FALSE },
	{ 7, "browser_pseudo_elements",	FALSE }
};
#define highlighting_properties_CSS		EMPTY_PROPERTIES


/* Cobol */
#define highlighting_lexer_COBOL		SCLEX_COBOL
#define highlighting_styles_COBOL		highlighting_styles_C
static const HLKeyword highlighting_keywords_COBOL[] =
{
	{ 0, "primary",				FALSE },
	{ 1, "secondary",			FALSE },
	{ 2, "extended_keywords",	FALSE }
};
#define highlighting_properties_COBOL	highlighting_properties_C


/* Conf */
#define highlighting_lexer_CONF			SCLEX_PROPERTIES
static const HLStyle highlighting_styles_CONF[] =
{
	{ SCE_PROPS_DEFAULT,	"default",		FALSE,	FALSE },
	{ SCE_PROPS_COMMENT,	"comment",		FALSE,	FALSE },
	{ SCE_PROPS_SECTION,	"section",		FALSE,	FALSE },
	{ SCE_PROPS_KEY,		"key",			FALSE,	FALSE },
	{ SCE_PROPS_ASSIGNMENT,	"assignment",	FALSE,	FALSE },
	{ SCE_PROPS_DEFVAL,		"defval",		FALSE,	FALSE }
};
#define highlighting_keywords_CONF		EMPTY_KEYWORDS
#define highlighting_properties_CONF	EMPTY_PROPERTIES


/* D */
#define highlighting_lexer_D		SCLEX_D
static const HLStyle highlighting_styles_D[] =
{
	{ SCE_D_DEFAULT,				"default",					FALSE,	FALSE },
	{ SCE_D_COMMENT,				"comment",					FALSE,	FALSE },
	{ SCE_D_COMMENTLINE,			"commentline",				FALSE,	FALSE },
	{ SCE_D_COMMENTDOC,				"commentdoc",				FALSE,	FALSE },
	{ SCE_D_COMMENTNESTED,			"commentnested",			FALSE,	FALSE },
	{ SCE_D_NUMBER,					"number",					FALSE,	FALSE },
	{ SCE_D_WORD,					"word",						FALSE,	FALSE },
	{ SCE_D_WORD2,					"word2",					FALSE,	FALSE },
	{ SCE_D_WORD3,					"word3",					FALSE,	FALSE },
	{ SCE_D_TYPEDEF,				"typedef",					FALSE,	FALSE }, /* FIXME: don't remap here */
	{ SCE_D_WORD5,					"typedef",					FALSE,	FALSE },
	{ SCE_D_STRING,					"string",					FALSE,	FALSE },
	{ SCE_D_STRINGB,				"string",					FALSE,	FALSE },
	{ SCE_D_STRINGR,				"string",					FALSE,	FALSE },
	{ SCE_D_STRINGEOL,				"stringeol",				FALSE,	FALSE },
	{ SCE_D_CHARACTER,				"character",				FALSE,	FALSE },
	{ SCE_D_OPERATOR,				"operator",					FALSE,	FALSE },
	{ SCE_D_IDENTIFIER,				"identifier",				FALSE,	FALSE },
	{ SCE_D_COMMENTLINEDOC,			"commentlinedoc",			FALSE,	FALSE },
	{ SCE_D_COMMENTDOCKEYWORD,		"commentdockeyword",		FALSE,	FALSE },
	{ SCE_D_COMMENTDOCKEYWORDERROR,	"commentdockeyworderror",	FALSE,	FALSE }
	/* these are for user-defined keywords we don't set yet */
	/*{ SCE_D_WORD6,					"word6",					FALSE,	FALSE },
	{ SCE_D_WORD7,					"word7",					FALSE,	FALSE }*/
};
static const HLKeyword highlighting_keywords_D[] =
{
	{ 0, "primary",		FALSE },
	/* SCI_SETKEYWORDS = 1 - secondary + global tags file types */
	{ 1, "secondary",	TRUE },
	{ 2, "docComment",	FALSE },
	/* SCI_SETKEYWORDS = 3 is for current session types - see document_highlight_tags() */
	{ 4, "types",		FALSE },
};
#define highlighting_properties_D		EMPTY_PROPERTIES


/* Diff */
#define highlighting_lexer_DIFF			SCLEX_DIFF
static const HLStyle highlighting_styles_DIFF[] =
{
	{ SCE_DIFF_DEFAULT,					"default",				FALSE,	FALSE },
	{ SCE_DIFF_COMMENT,					"comment",				FALSE,	FALSE },
	{ SCE_DIFF_COMMAND,					"command",				FALSE,	FALSE },
	{ SCE_DIFF_HEADER,					"header",				FALSE,	FALSE },
	{ SCE_DIFF_POSITION,				"position",				FALSE,	FALSE },
	{ SCE_DIFF_DELETED,					"deleted",				FALSE,	FALSE },
	{ SCE_DIFF_ADDED,					"added",				FALSE,	FALSE },
	{ SCE_DIFF_CHANGED,					"changed",				FALSE,	FALSE },
	{ SCE_DIFF_PATCH_ADD,				"patch_add",			FALSE,	FALSE },
	{ SCE_DIFF_PATCH_DELETE,			"patch_delete",			FALSE,	FALSE },
	{ SCE_DIFF_REMOVED_PATCH_ADD,		"removed_patch_add",	FALSE,	FALSE },
	{ SCE_DIFF_REMOVED_PATCH_DELETE,	"removed_patch_delete",	FALSE,	FALSE }
};
#define highlighting_keywords_DIFF		EMPTY_KEYWORDS
#define highlighting_properties_DIFF	EMPTY_PROPERTIES


#define highlighting_lexer_DOCBOOK			SCLEX_XML
static const HLStyle highlighting_styles_DOCBOOK[] =
{
	{ SCE_H_DEFAULT,				"default",					FALSE,	FALSE },
	{ SCE_H_TAG,					"tag",						FALSE,	FALSE },
	{ SCE_H_TAGUNKNOWN,				"tagunknown",				FALSE,	FALSE },
	{ SCE_H_ATTRIBUTE,				"attribute",				FALSE,	FALSE },
	{ SCE_H_ATTRIBUTEUNKNOWN,		"attributeunknown",			FALSE,	FALSE },
	{ SCE_H_NUMBER,					"number",					FALSE,	FALSE },
	{ SCE_H_DOUBLESTRING,			"doublestring",				FALSE,	FALSE },
	{ SCE_H_SINGLESTRING,			"singlestring",				FALSE,	FALSE },
	{ SCE_H_OTHER,					"other",					FALSE,	FALSE },
	{ SCE_H_COMMENT,				"comment",					FALSE,	FALSE },
	{ SCE_H_ENTITY,					"entity",					FALSE,	FALSE },
	{ SCE_H_TAGEND,					"tagend",					FALSE,	FALSE },
	{ SCE_H_XMLSTART,				"xmlstart",					TRUE,	FALSE },
	{ SCE_H_XMLEND,					"xmlend",					FALSE,	FALSE },
	{ SCE_H_CDATA,					"cdata",					FALSE,	FALSE },
	{ SCE_H_QUESTION,				"question",					FALSE,	FALSE },
	{ SCE_H_VALUE,					"value",					FALSE,	FALSE },
	{ SCE_H_XCCOMMENT,				"xccomment",				FALSE,	FALSE },
	{ SCE_H_SGML_DEFAULT,			"sgml_default",				FALSE,	FALSE },
	{ SCE_H_SGML_COMMENT,			"sgml_comment",				FALSE,	FALSE },
	{ SCE_H_SGML_SPECIAL,			"sgml_special",				FALSE,	FALSE },
	{ SCE_H_SGML_COMMAND,			"sgml_command",				FALSE,	FALSE },
	{ SCE_H_SGML_DOUBLESTRING,		"sgml_doublestring",		FALSE,	FALSE },
	{ SCE_H_SGML_SIMPLESTRING,		"sgml_simplestring",		FALSE,	FALSE },
	{ SCE_H_SGML_1ST_PARAM,			"sgml_1st_param",			FALSE,	FALSE },
	{ SCE_H_SGML_ENTITY,			"sgml_entity",				FALSE,	FALSE },
	{ SCE_H_SGML_BLOCK_DEFAULT,		"sgml_block_default",		FALSE,	FALSE },
	{ SCE_H_SGML_1ST_PARAM_COMMENT,	"sgml_1st_param_comment",	FALSE,	FALSE },
	{ SCE_H_SGML_ERROR,				"sgml_error",				FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_DOCBOOK[] =
{
	{ 0, "elements",	FALSE },
	{ 5, "dtd",			FALSE }
};
#define highlighting_properties_DOCBOOK		EMPTY_PROPERTIES


/* Erlang */
#define highlighting_lexer_ERLANG		SCLEX_ERLANG
static const HLStyle highlighting_styles_ERLANG[] =
{
	{ SCE_ERLANG_DEFAULT,			"default",				FALSE,	FALSE },
	{ SCE_ERLANG_COMMENT,			"comment",				FALSE,	FALSE },
	{ SCE_ERLANG_VARIABLE,			"variable",				FALSE,	FALSE },
	{ SCE_ERLANG_NUMBER,			"number",				FALSE,	FALSE },
	{ SCE_ERLANG_KEYWORD,			"keyword",				FALSE,	FALSE },
	{ SCE_ERLANG_STRING,			"string",				FALSE,	FALSE },
	{ SCE_ERLANG_OPERATOR,			"operator",				FALSE,	FALSE },
	{ SCE_ERLANG_ATOM,				"atom",					FALSE,	FALSE },
	{ SCE_ERLANG_FUNCTION_NAME,		"function_name",		FALSE,	FALSE },
	{ SCE_ERLANG_CHARACTER,			"character",			FALSE,	FALSE },
	{ SCE_ERLANG_MACRO,				"macro",				FALSE,	FALSE },
	{ SCE_ERLANG_RECORD,			"record",				FALSE,	FALSE },
	{ SCE_ERLANG_PREPROC,			"preproc",				FALSE,	FALSE },
	{ SCE_ERLANG_NODE_NAME,			"node_name",			FALSE,	FALSE },
	{ SCE_ERLANG_COMMENT_FUNCTION,	"comment_function",		FALSE,	FALSE },
	{ SCE_ERLANG_COMMENT_MODULE,	"comment_module",		FALSE,	FALSE },
	{ SCE_ERLANG_COMMENT_DOC,		"comment_doc",			FALSE,	FALSE },
	{ SCE_ERLANG_COMMENT_DOC_MACRO,	"comment_doc_macro",	FALSE,	FALSE },
	{ SCE_ERLANG_ATOM_QUOTED,		"atom_quoted",			FALSE,	FALSE },
	{ SCE_ERLANG_MACRO_QUOTED,		"macro_quoted",			FALSE,	FALSE },
	{ SCE_ERLANG_RECORD_QUOTED,		"record_quoted",		FALSE,	FALSE },
	{ SCE_ERLANG_NODE_NAME_QUOTED,	"node_name_quoted",		FALSE,	FALSE },
	{ SCE_ERLANG_BIFS,				"bifs",					FALSE,	FALSE },
	{ SCE_ERLANG_MODULES,			"modules",				FALSE,	FALSE },
	{ SCE_ERLANG_MODULES_ATT,		"modules_att",			FALSE,	FALSE },
	{ SCE_ERLANG_UNKNOWN,			"unknown",				FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_ERLANG[] =
{
	{ 0, "keywords",	FALSE },
	{ 1, "bifs",		FALSE },
	{ 2, "preproc",		FALSE },
	{ 3, "module",		FALSE },
	{ 4, "doc",			FALSE },
	{ 5, "doc_macro",	FALSE }
};
#define highlighting_properties_ERLANG	EMPTY_PROPERTIES


/* F77 */
#define highlighting_lexer_F77			SCLEX_F77
static const HLStyle highlighting_styles_F77[] =
{
	{ SCE_F_DEFAULT,		"default",			FALSE,	FALSE },
	{ SCE_F_COMMENT,		"comment",			FALSE,	FALSE },
	{ SCE_F_NUMBER,			"number",			FALSE,	FALSE },
	{ SCE_F_STRING1,		"string",			FALSE,	FALSE },
	{ SCE_F_OPERATOR,		"operator",			FALSE,	FALSE },
	{ SCE_F_IDENTIFIER,		"identifier",		FALSE,	FALSE },
	{ SCE_F_STRING2,		"string2",			FALSE,	FALSE },
	{ SCE_F_WORD,			"word",				FALSE,	FALSE },
	{ SCE_F_WORD2,			"word2",			FALSE,	FALSE },
	{ SCE_F_WORD3,			"word3",			FALSE,	FALSE },
	{ SCE_F_PREPROCESSOR,	"preprocessor",		FALSE,	FALSE },
	{ SCE_F_OPERATOR2,		"operator2",		FALSE,	FALSE },
	{ SCE_F_CONTINUATION,	"continuation",		FALSE,	FALSE },
	{ SCE_F_STRINGEOL,		"stringeol",		FALSE,	FALSE },
	{ SCE_F_LABEL,			"label",			FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_F77[] =
{
	{ 0, "primary",				FALSE },
	{ 1, "intrinsic_functions",	FALSE },
	{ 2, "user_functions",		FALSE }
};
#define highlighting_properties_F77		EMPTY_PROPERTIES


/* Forth */
#define highlighting_lexer_FORTH		SCLEX_FORTH
static const HLStyle highlighting_styles_FORTH[] =
{
	{ SCE_FORTH_DEFAULT,	"default",		FALSE,	FALSE },
	{ SCE_FORTH_COMMENT,	"comment",		FALSE,	FALSE },
	{ SCE_FORTH_COMMENT_ML,	"commentml",	FALSE,	FALSE },
	{ SCE_FORTH_IDENTIFIER,	"identifier",	FALSE,	FALSE },
	{ SCE_FORTH_CONTROL,	"control",		FALSE,	FALSE },
	{ SCE_FORTH_KEYWORD,	"keyword",		FALSE,	FALSE },
	{ SCE_FORTH_DEFWORD,	"defword",		FALSE,	FALSE },
	{ SCE_FORTH_PREWORD1,	"preword1",		FALSE,	FALSE },
	{ SCE_FORTH_PREWORD2,	"preword2",		FALSE,	FALSE },
	{ SCE_FORTH_NUMBER,		"number",		FALSE,	FALSE },
	{ SCE_FORTH_STRING,		"string",		FALSE,	FALSE },
	{ SCE_FORTH_LOCALE,		"locale",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_FORTH[] =
{
	{ 0, "primary",		FALSE },
	{ 1, "keyword",		FALSE },
	{ 2, "defword",		FALSE },
	{ 3, "preword1",	FALSE },
	{ 4, "preword2",	FALSE },
	{ 5, "string",		FALSE }
};
#define highlighting_properties_FORTH	EMPTY_PROPERTIES


/* Fortran */
/* F77 and Fortran (F9x) uses different lexers but shares styles and keywords */
#define highlighting_lexer_FORTRAN			SCLEX_FORTRAN
#define highlighting_styles_FORTRAN			highlighting_styles_F77
#define highlighting_keywords_FORTRAN		highlighting_keywords_F77
#define highlighting_properties_FORTRAN		highlighting_properties_F77


/* GDScript */
#define highlighting_lexer_GDSCRIPT		SCLEX_GDSCRIPT
static const HLStyle highlighting_styles_GDSCRIPT[] =
{
	{ SCE_GD_DEFAULT,		"default",			FALSE,	FALSE },
	{ SCE_GD_COMMENTLINE,	"commentline",		FALSE,	FALSE },
	{ SCE_GD_NUMBER,		"number",			FALSE,	FALSE },
	{ SCE_GD_STRING,		"string",			FALSE,	FALSE },
	{ SCE_GD_CHARACTER,		"character",		FALSE,	FALSE },
	{ SCE_GD_WORD,			"word",				FALSE,	FALSE },
	{ SCE_GD_TRIPLE,		"triple",			FALSE,	FALSE },
	{ SCE_GD_TRIPLEDOUBLE,	"tripledouble",		FALSE,	FALSE },
	{ SCE_GD_CLASSNAME,		"classname",		FALSE,	FALSE },
	{ SCE_GD_FUNCNAME,		"funcname",			FALSE,	FALSE },
	{ SCE_GD_OPERATOR,		"operator",			FALSE,	FALSE },
	{ SCE_GD_IDENTIFIER,	"identifier",		FALSE,	TRUE },
	{ SCE_GD_COMMENTBLOCK,	"commentblock",		FALSE,	FALSE },
	{ SCE_GD_STRINGEOL,		"stringeol",		FALSE,	FALSE },
	{ SCE_GD_WORD2,			"word2",			FALSE,	FALSE },
	{ SCE_GD_ANNOTATION,	"annotation",		FALSE,	FALSE },
	{ SCE_GD_NODEPATH,		"notepath",			FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_GDSCRIPT[] =
{
	{ 0, "primary",		FALSE },
	{ 1, "identifiers",	FALSE }
};
#define highlighting_properties_GDSCRIPT	EMPTY_PROPERTIES


/* Go */
#define highlighting_lexer_GO		SCLEX_CPP
#define highlighting_styles_GO		highlighting_styles_C
#define highlighting_keywords_GO	highlighting_keywords_C
#define highlighting_properties_GO	highlighting_properties_C


/* Haskell */
#define highlighting_lexer_HASKELL			SCLEX_HASKELL
static const HLStyle highlighting_styles_HASKELL[] =
{
	{ SCE_HA_DEFAULT,				"default",				FALSE,	FALSE },
	{ SCE_HA_COMMENTLINE,			"commentline",			FALSE,	FALSE },
	{ SCE_HA_COMMENTBLOCK,			"commentblock",			FALSE,	FALSE },
	{ SCE_HA_COMMENTBLOCK2,			"commentblock2",		FALSE,	FALSE },
	{ SCE_HA_COMMENTBLOCK3,			"commentblock3",		FALSE,	FALSE },
	{ SCE_HA_NUMBER,				"number",				FALSE,	FALSE },
	{ SCE_HA_KEYWORD,				"keyword",				FALSE,	FALSE },
	{ SCE_HA_IMPORT,				"import",				FALSE,	FALSE },
	{ SCE_HA_STRING,				"string",				FALSE,	FALSE },
	{ SCE_HA_CHARACTER,				"character",			FALSE,	FALSE },
	{ SCE_HA_CLASS,					"class",				FALSE,	FALSE },
	{ SCE_HA_OPERATOR,				"operator",				FALSE,	FALSE },
	{ SCE_HA_IDENTIFIER,			"identifier",			FALSE,	FALSE },
	{ SCE_HA_INSTANCE,				"instance",				FALSE,	FALSE },
	{ SCE_HA_CAPITAL,				"capital",				FALSE,	FALSE },
	{ SCE_HA_MODULE,				"module",				FALSE,	FALSE },
	{ SCE_HA_DATA,					"data",					FALSE,	FALSE },
	{ SCE_HA_PRAGMA,				"pragma",				FALSE,	FALSE },
	{ SCE_HA_PREPROCESSOR,			"preprocessor",			FALSE,	FALSE },
	{ SCE_HA_STRINGEOL,				"stringeol",			FALSE,	FALSE },
	{ SCE_HA_RESERVED_OPERATOR,		"reserved_operator",	FALSE,	FALSE },
	{ SCE_HA_LITERATE_COMMENT,		"literate_comment",		FALSE,	FALSE },
	{ SCE_HA_LITERATE_CODEDELIM,	"literate_codedelim",	FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_HASKELL[] =
{
	{ 0, "keywords",		   FALSE },
	{ 1, "ffi",				   FALSE },
	{ 2, "reserved_operators", FALSE }
};
#define highlighting_properties_HASKELL		EMPTY_PROPERTIES


/* HAXE */
#define highlighting_lexer_HAXE			SCLEX_CPP
#define highlighting_styles_HAXE		highlighting_styles_C
static const HLKeyword highlighting_keywords_HAXE[] =
{
	{ 0, "primary",		FALSE },
	{ 1, "secondary",	FALSE },
	{ 3, "classes",		FALSE }
};
#define highlighting_properties_HAXE	highlighting_properties_C


/* HTML */
#define highlighting_lexer_HTML		SCLEX_HTML
static const HLStyle highlighting_styles_HTML[] =
{
	{ SCE_H_DEFAULT,				"html_default",				FALSE,	FALSE },
	{ SCE_H_TAG,					"html_tag",					FALSE,	FALSE },
	{ SCE_H_TAGUNKNOWN,				"html_tagunknown",			FALSE,	FALSE },
	{ SCE_H_ATTRIBUTE,				"html_attribute",			FALSE,	FALSE },
	{ SCE_H_ATTRIBUTEUNKNOWN,		"html_attributeunknown",	FALSE,	FALSE },
	{ SCE_H_NUMBER,					"html_number",				FALSE,	FALSE },
	{ SCE_H_DOUBLESTRING,			"html_doublestring",		FALSE,	FALSE },
	{ SCE_H_SINGLESTRING,			"html_singlestring",		FALSE,	FALSE },
	{ SCE_H_OTHER,					"html_other",				FALSE,	FALSE },
	{ SCE_H_COMMENT,				"html_comment",				FALSE,	FALSE },
	{ SCE_H_ENTITY,					"html_entity",				FALSE,	FALSE },
	{ SCE_H_TAGEND,					"html_tagend",				FALSE,	FALSE },
	{ SCE_H_XMLSTART,				"html_xmlstart", 			TRUE,	FALSE },
	{ SCE_H_XMLEND,					"html_xmlend",				FALSE,	FALSE },
	{ SCE_H_SCRIPT,					"html_script",				FALSE,	FALSE },
	{ SCE_H_ASP,					"html_asp",					TRUE,	FALSE },
	{ SCE_H_ASPAT,					"html_aspat",				TRUE,	FALSE },
	{ SCE_H_CDATA,					"html_cdata",				FALSE,	FALSE },
	{ SCE_H_QUESTION,				"html_question",			FALSE,	FALSE },
	{ SCE_H_VALUE,					"html_value",				FALSE,	FALSE },
	{ SCE_H_XCCOMMENT,				"html_xccomment",			FALSE,	FALSE },

	{ SCE_H_SGML_DEFAULT,			"sgml_default",				FALSE,	FALSE },
	{ SCE_H_SGML_COMMENT,			"sgml_comment",				FALSE,	FALSE },
	{ SCE_H_SGML_SPECIAL,			"sgml_special",				FALSE,	FALSE },
	{ SCE_H_SGML_COMMAND,			"sgml_command",				FALSE,	FALSE },
	{ SCE_H_SGML_DOUBLESTRING,		"sgml_doublestring",		FALSE,	FALSE },
	{ SCE_H_SGML_SIMPLESTRING,		"sgml_simplestring",		FALSE,	FALSE },
	{ SCE_H_SGML_1ST_PARAM,			"sgml_1st_param",			FALSE,	FALSE },
	{ SCE_H_SGML_ENTITY,			"sgml_entity",				FALSE,	FALSE },
	{ SCE_H_SGML_BLOCK_DEFAULT,		"sgml_block_default",		FALSE,	FALSE },
	{ SCE_H_SGML_1ST_PARAM_COMMENT,	"sgml_1st_param_comment",	FALSE,	FALSE },
	{ SCE_H_SGML_ERROR,				"sgml_error",				FALSE,	FALSE },

	/* embedded JavaScript */
	{ SCE_HJ_START,					"jscript_start",			FALSE,	FALSE },
	{ SCE_HJ_DEFAULT,				"jscript_default",			FALSE,	FALSE },
	{ SCE_HJ_COMMENT,				"jscript_comment",			FALSE,	FALSE },
	{ SCE_HJ_COMMENTLINE,			"jscript_commentline",		FALSE,	FALSE },
	{ SCE_HJ_COMMENTDOC,			"jscript_commentdoc",		FALSE,	FALSE },
	{ SCE_HJ_NUMBER,				"jscript_number",			FALSE,	FALSE },
	{ SCE_HJ_WORD,					"jscript_word",				FALSE,	FALSE },
	{ SCE_HJ_KEYWORD,				"jscript_keyword",			FALSE,	FALSE },
	{ SCE_HJ_DOUBLESTRING,			"jscript_doublestring",		FALSE,	FALSE },
	{ SCE_HJ_SINGLESTRING,			"jscript_singlestring",		FALSE,	FALSE },
	{ SCE_HJ_SYMBOLS,				"jscript_symbols",			FALSE,	FALSE },
	{ SCE_HJ_STRINGEOL,				"jscript_stringeol",		FALSE,	FALSE },
	{ SCE_HJ_REGEX,					"jscript_regex",			FALSE,	FALSE },

	/* for HB, VBScript?, use the same styles as for JavaScript */
	{ SCE_HB_START,					"jscript_start",			FALSE,	FALSE },
	{ SCE_HB_DEFAULT,				"jscript_default",			FALSE,	FALSE },
	{ SCE_HB_COMMENTLINE,			"jscript_commentline",		FALSE,	FALSE },
	{ SCE_HB_NUMBER,				"jscript_number",			FALSE,	FALSE },
	{ SCE_HB_WORD,					"jscript_keyword",			FALSE,	FALSE }, /* keywords */
	{ SCE_HB_STRING,				"jscript_doublestring",		FALSE,	FALSE },
	{ SCE_HB_IDENTIFIER,			"jscript_symbols",			FALSE,	FALSE },
	{ SCE_HB_STRINGEOL,				"jscript_stringeol",		FALSE,	FALSE },

	/* for HBA, VBScript?, use the same styles as for JavaScript */
	{ SCE_HBA_START,				"jscript_start",			FALSE,	FALSE },
	{ SCE_HBA_DEFAULT,				"jscript_default",			FALSE,	FALSE },
	{ SCE_HBA_COMMENTLINE,			"jscript_commentline",		FALSE,	FALSE },
	{ SCE_HBA_NUMBER,				"jscript_number",			FALSE,	FALSE },
	{ SCE_HBA_WORD,					"jscript_keyword",			FALSE,	FALSE }, /* keywords */
	{ SCE_HBA_STRING,				"jscript_doublestring",		FALSE,	FALSE },
	{ SCE_HBA_IDENTIFIER,			"jscript_symbols",			FALSE,	FALSE },
	{ SCE_HBA_STRINGEOL,			"jscript_stringeol",		FALSE,	FALSE },

	/* for HJA, ASP Javascript, use the same styles as for JavaScript */
	{ SCE_HJA_START,				"jscript_start",			FALSE,	FALSE },
	{ SCE_HJA_DEFAULT,				"jscript_default",			FALSE,	FALSE },
	{ SCE_HJA_COMMENT,				"jscript_comment",			FALSE,	FALSE },
	{ SCE_HJA_COMMENTLINE,			"jscript_commentline",		FALSE,	FALSE },
	{ SCE_HJA_COMMENTDOC,			"jscript_commentdoc",		FALSE,	FALSE },
	{ SCE_HJA_NUMBER,				"jscript_number",			FALSE,	FALSE },
	{ SCE_HJA_WORD,					"jscript_word",				FALSE,	FALSE },
	{ SCE_HJA_KEYWORD,				"jscript_keyword",			FALSE,	FALSE },
	{ SCE_HJA_DOUBLESTRING,			"jscript_doublestring",		FALSE,	FALSE },
	{ SCE_HJA_SINGLESTRING,			"jscript_singlestring",		FALSE,	FALSE },
	{ SCE_HJA_SYMBOLS,				"jscript_symbols",			FALSE,	FALSE },
	{ SCE_HJA_STRINGEOL,			"jscript_stringeol",		FALSE,	FALSE },
	{ SCE_HJA_REGEX,				"jscript_regex",			FALSE,	FALSE },

	/* embedded Python */
	{ SCE_HP_START,					"jscript_start",			FALSE,	FALSE },
	{ SCE_HP_DEFAULT,				"python_default",			FALSE,	FALSE },
	{ SCE_HP_COMMENTLINE,			"python_commentline",		FALSE,	FALSE },
	{ SCE_HP_NUMBER,				"python_number",			FALSE,	FALSE },
	{ SCE_HP_STRING,				"python_string",			FALSE,	FALSE },
	{ SCE_HP_CHARACTER,				"python_character",			FALSE,	FALSE },
	{ SCE_HP_WORD,					"python_word",				FALSE,	FALSE },
	{ SCE_HP_TRIPLE,				"python_triple",			FALSE,	FALSE },
	{ SCE_HP_TRIPLEDOUBLE,			"python_tripledouble",		FALSE,	FALSE },
	{ SCE_HP_CLASSNAME,				"python_classname",			FALSE,	FALSE },
	{ SCE_HP_DEFNAME,				"python_defname",			FALSE,	FALSE },
	{ SCE_HP_OPERATOR,				"python_operator",			FALSE,	FALSE },
	{ SCE_HP_IDENTIFIER,			"python_identifier",		FALSE,	FALSE },

	/* for embedded HPA (what is this?) we use the Python styles */
	{ SCE_HPA_START,				"jscript_start",			FALSE,	FALSE },
	{ SCE_HPA_DEFAULT,				"python_default",			FALSE,	FALSE },
	{ SCE_HPA_COMMENTLINE,			"python_commentline",		FALSE,	FALSE },
	{ SCE_HPA_NUMBER,				"python_number",			FALSE,	FALSE },
	{ SCE_HPA_STRING,				"python_string",			FALSE,	FALSE },
	{ SCE_HPA_CHARACTER,			"python_character",			FALSE,	FALSE },
	{ SCE_HPA_WORD,					"python_word",				FALSE,	FALSE },
	{ SCE_HPA_TRIPLE,				"python_triple",			FALSE,	FALSE },
	{ SCE_HPA_TRIPLEDOUBLE,			"python_tripledouble",		FALSE,	FALSE },
	{ SCE_HPA_CLASSNAME,			"python_classname",			FALSE,	FALSE },
	{ SCE_HPA_DEFNAME,				"python_defname",			FALSE,	FALSE },
	{ SCE_HPA_OPERATOR,				"python_operator",			FALSE,	FALSE },
	{ SCE_HPA_IDENTIFIER,			"python_identifier",		FALSE,	FALSE },

	/* PHP */
	{ SCE_HPHP_DEFAULT,				"php_default",				FALSE,	FALSE },
	{ SCE_HPHP_SIMPLESTRING,		"php_simplestring",			FALSE,	FALSE },
	{ SCE_HPHP_HSTRING,				"php_hstring",				FALSE,	FALSE },
	{ SCE_HPHP_NUMBER,				"php_number",				FALSE,	FALSE },
	{ SCE_HPHP_WORD,				"php_word",					FALSE,	FALSE },
	{ SCE_HPHP_VARIABLE,			"php_variable",				FALSE,	FALSE },
	{ SCE_HPHP_COMMENT,				"php_comment",				FALSE,	FALSE },
	{ SCE_HPHP_COMMENTLINE,			"php_commentline",			FALSE,	FALSE },
	{ SCE_HPHP_OPERATOR,			"php_operator",				FALSE,	FALSE },
	{ SCE_HPHP_HSTRING_VARIABLE,	"php_hstring_variable",		FALSE,	FALSE },
	{ SCE_HPHP_COMPLEX_VARIABLE,	"php_complex_variable",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_HTML[] =
{
	{ 0, "html",		FALSE },
	{ 1, "javascript",	FALSE },
	{ 2, "vbscript",	FALSE },
	{ 3, "python",		FALSE },
	{ 4, "php",			FALSE },
	{ 5, "sgml",		FALSE }
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
	{ 0, "primary",		FALSE },
	/* SCI_SETKEYWORDS = 1 - secondary + global tags file types, see below */
	{ 1, "secondary",	TRUE },
	{ 2, "doccomment",	FALSE },
	/* SCI_SETKEYWORDS = 3 is for current session types - see document_highlight_tags() */
	{ 4, "typedefs",	FALSE }
};
#define highlighting_properties_JAVA	highlighting_properties_C


/* JavaScript */
#define highlighting_lexer_JS		SCLEX_CPP
#define highlighting_styles_JS		highlighting_styles_C
static const HLKeyword highlighting_keywords_JS[] =
{
	{ 0, "primary",		FALSE },
	{ 1, "secondary",	FALSE }
};
#define highlighting_properties_JS	highlighting_properties_C

/* Julia */
#define highlighting_lexer_JULIA		SCLEX_JULIA
static const HLStyle highlighting_styles_JULIA[] =
{
	{ SCE_JULIA_DEFAULT,			"default",				FALSE,	FALSE },
	{ SCE_JULIA_COMMENT,			"comment",				FALSE,	FALSE },
	{ SCE_JULIA_NUMBER,				"number",				FALSE,	FALSE },
	{ SCE_JULIA_KEYWORD1,			"keyword1",				FALSE,	FALSE },
	{ SCE_JULIA_KEYWORD2,			"keyword2",				FALSE,	FALSE },
	{ SCE_JULIA_KEYWORD3,			"keyword3",				FALSE,	FALSE },
	{ SCE_JULIA_CHAR,				"char",					FALSE,	FALSE },
	{ SCE_JULIA_OPERATOR,			"operator",				FALSE,	FALSE },
	{ SCE_JULIA_BRACKET,			"bracket",				FALSE,	FALSE },
	{ SCE_JULIA_IDENTIFIER,			"identifier",			FALSE,	FALSE },
	{ SCE_JULIA_STRING,				"string",				FALSE,	FALSE },
	{ SCE_JULIA_SYMBOL,				"symbol",				FALSE,	FALSE },
	{ SCE_JULIA_MACRO,				"macro",				FALSE,	FALSE },
	{ SCE_JULIA_STRINGINTERP,		"stringinterp",			FALSE,	FALSE },
	{ SCE_JULIA_DOCSTRING,			"docstring",			FALSE,	FALSE },
	{ SCE_JULIA_STRINGLITERAL,		"stringliteral",		FALSE,	FALSE },
	{ SCE_JULIA_COMMAND,			"command",				FALSE,	FALSE },
	{ SCE_JULIA_COMMANDLITERAL,		"commandliteral",		FALSE,	FALSE },
	{ SCE_JULIA_TYPEANNOT,			"typeannotation",		FALSE,	FALSE },
	{ SCE_JULIA_LEXERROR,			"lexerror",				FALSE,	FALSE },
	{ SCE_JULIA_KEYWORD4,			"keyword4",				FALSE,	FALSE },
	{ SCE_JULIA_TYPEOPERATOR,		"typeoperator",			FALSE,	FALSE },
};
static const HLKeyword highlighting_keywords_JULIA[] =
{
	{ 0, "primary",		FALSE },
	{ 1, "secondary",	FALSE },
	{ 2, "tertiary",	FALSE },
	{ 3, "functions",	FALSE }
};
#define highlighting_properties_JULIA	EMPTY_PROPERTIES


/* LaTeX */
#define highlighting_lexer_LATEX		SCLEX_LATEX
static const HLStyle highlighting_styles_LATEX[] =
{
	{ SCE_L_DEFAULT,	"default",		FALSE,	FALSE },
	{ SCE_L_COMMAND,	"command",		FALSE,	FALSE },
	{ SCE_L_TAG,		"tag",			FALSE,	FALSE },
	{ SCE_L_MATH,		"math",			FALSE,	FALSE },
	{ SCE_L_COMMENT,	"comment",		FALSE,	FALSE },
	{ SCE_L_TAG2,		"tag2",			FALSE,	FALSE },
	{ SCE_L_MATH2,		"math2",		FALSE,	FALSE },
	{ SCE_L_COMMENT2,	"comment2",		FALSE,	FALSE },
	{ SCE_L_VERBATIM,	"verbatim",		FALSE,	FALSE },
	{ SCE_L_SHORTCMD,	"shortcmd",		FALSE,	FALSE },
	{ SCE_L_SPECIAL,	"special",		FALSE,	FALSE },
	{ SCE_L_CMDOPT,		"cmdopt",		FALSE,	FALSE },
	{ SCE_L_ERROR,		"error",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_LATEX[] =
{
	{ 0, "primary",	FALSE }
};
#define highlighting_properties_LATEX	EMPTY_PROPERTIES


/* Lisp */
#define highlighting_lexer_LISP			SCLEX_LISP
static const HLStyle highlighting_styles_LISP[] =
{
	{ SCE_LISP_DEFAULT,			"default",			FALSE,	FALSE },
	{ SCE_LISP_COMMENT,			"comment",			FALSE,	FALSE },
	{ SCE_LISP_MULTI_COMMENT,	"multicomment",		FALSE,	FALSE },
	{ SCE_LISP_NUMBER,			"number",			FALSE,	FALSE },
	{ SCE_LISP_KEYWORD,			"keyword",			FALSE,	FALSE },
	{ SCE_LISP_SYMBOL,			"symbol",			FALSE,	FALSE },
	{ SCE_LISP_STRING,			"string",			FALSE,	FALSE },
	{ SCE_LISP_STRINGEOL,		"stringeol",		FALSE,	FALSE },
	{ SCE_LISP_IDENTIFIER,		"identifier",		FALSE,	FALSE },
	{ SCE_LISP_OPERATOR,		"operator",			FALSE,	FALSE },
	{ SCE_LISP_SPECIAL,			"special",			FALSE,	FALSE },
	{ SCE_LISP_KEYWORD_KW,		"keywordkw",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_LISP[] =
{
	{ 0, "keywords",			FALSE },
	{ 1, "special_keywords",	FALSE }
};
#define highlighting_properties_LISP	EMPTY_PROPERTIES


/* Lua */
#define highlighting_lexer_LUA			SCLEX_LUA
static const HLStyle highlighting_styles_LUA[] =
{
	{ SCE_LUA_DEFAULT,			"default",			FALSE,	FALSE },
	{ SCE_LUA_COMMENT,			"comment",			FALSE,	FALSE },
	{ SCE_LUA_COMMENTLINE,		"commentline",		FALSE,	FALSE },
	{ SCE_LUA_COMMENTDOC,		"commentdoc",		FALSE,	FALSE },
	{ SCE_LUA_NUMBER,			"number",			FALSE,	FALSE },
	{ SCE_LUA_WORD,				"word",				FALSE,	FALSE },
	{ SCE_LUA_STRING,			"string",			FALSE,	FALSE },
	{ SCE_LUA_CHARACTER,		"character",		FALSE,	FALSE },
	{ SCE_LUA_LITERALSTRING,	"literalstring",	FALSE,	FALSE },
	{ SCE_LUA_PREPROCESSOR,		"preprocessor",		FALSE,	FALSE },
	{ SCE_LUA_OPERATOR,			"operator",			FALSE,	FALSE },
	{ SCE_LUA_IDENTIFIER,		"identifier",		FALSE,	FALSE },
	{ SCE_LUA_STRINGEOL,		"stringeol",		FALSE,	FALSE },
	{ SCE_LUA_WORD2,			"function_basic",	FALSE,	FALSE },
	{ SCE_LUA_WORD3,			"function_other",	FALSE,	FALSE },
	{ SCE_LUA_WORD4,			"coroutines",		FALSE,	FALSE },
	{ SCE_LUA_WORD5,			"word5",			FALSE,	FALSE },
	{ SCE_LUA_WORD6,			"word6",			FALSE,	FALSE },
	{ SCE_LUA_WORD7,			"word7",			FALSE,	FALSE },
	{ SCE_LUA_WORD8,			"word8",			FALSE,	FALSE },
	{ SCE_LUA_LABEL,			"label",			FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_LUA[] =
{
	{ 0, "keywords",		FALSE },
	{ 1, "function_basic",	FALSE },
	{ 2, "function_other",	FALSE },
	{ 3, "coroutines",		FALSE },
	{ 4, "user1",			FALSE },
	{ 5, "user2",			FALSE },
	{ 6, "user3",			FALSE },
	{ 7, "user4",			FALSE }
};
#define highlighting_properties_LUA		EMPTY_PROPERTIES


/* Makefile */
#define highlighting_lexer_MAKE			SCLEX_MAKEFILE
static const HLStyle highlighting_styles_MAKE[] =
{
	{ SCE_MAKE_DEFAULT,			"default",		FALSE,	FALSE },
	{ SCE_MAKE_COMMENT,			"comment",		FALSE,	FALSE },
	{ SCE_MAKE_PREPROCESSOR,	"preprocessor",	FALSE,	FALSE },
	{ SCE_MAKE_IDENTIFIER,		"identifier",	FALSE,	FALSE },
	{ SCE_MAKE_OPERATOR,		"operator",		FALSE,	FALSE },
	{ SCE_MAKE_TARGET,			"target",		FALSE,	FALSE },
	{ SCE_MAKE_IDEOL,			"ideol",		FALSE,	FALSE }
};
#define highlighting_keywords_MAKE		EMPTY_KEYWORDS
#define highlighting_properties_MAKE	EMPTY_PROPERTIES


/* Markdown */
#define highlighting_lexer_MARKDOWN			SCLEX_MARKDOWN
static const HLStyle highlighting_styles_MARKDOWN[] =
{
	{ SCE_MARKDOWN_DEFAULT,		"default",		FALSE,	FALSE },
	{ SCE_MARKDOWN_LINE_BEGIN,	"default",		FALSE,	FALSE }, /* FIXME: avoid in-code re-mappings */
	{ SCE_MARKDOWN_PRECHAR,		"default",		FALSE,	FALSE },
	{ SCE_MARKDOWN_STRONG1,		"strong",		FALSE,	FALSE },
	{ SCE_MARKDOWN_STRONG2,		"strong",		FALSE,	FALSE },
	{ SCE_MARKDOWN_EM1,			"emphasis",		FALSE,	FALSE },
	{ SCE_MARKDOWN_EM2,			"emphasis",		FALSE,	FALSE },
	{ SCE_MARKDOWN_HEADER1,		"header1",		FALSE,	FALSE },
	{ SCE_MARKDOWN_HEADER2,		"header2",		FALSE,	FALSE },
	{ SCE_MARKDOWN_HEADER3,		"header3",		FALSE,	FALSE },
	{ SCE_MARKDOWN_HEADER4,		"header4",		FALSE,	FALSE },
	{ SCE_MARKDOWN_HEADER5,		"header5",		FALSE,	FALSE },
	{ SCE_MARKDOWN_HEADER6,		"header6",		FALSE,	FALSE },
	{ SCE_MARKDOWN_ULIST_ITEM,	"ulist_item",	FALSE,	FALSE },
	{ SCE_MARKDOWN_OLIST_ITEM,	"olist_item",	FALSE,	FALSE },
	{ SCE_MARKDOWN_BLOCKQUOTE,	"blockquote",	FALSE,	FALSE },
	{ SCE_MARKDOWN_STRIKEOUT,	"strikeout",	FALSE,	FALSE },
	{ SCE_MARKDOWN_HRULE,		"hrule",		FALSE,	FALSE },
	{ SCE_MARKDOWN_LINK,		"link",			FALSE,	FALSE },
	{ SCE_MARKDOWN_CODE,		"code",			FALSE,	FALSE },
	{ SCE_MARKDOWN_CODE2,		"code",			FALSE,	FALSE },
	{ SCE_MARKDOWN_CODEBK,		"codebk",		FALSE,	FALSE }
};
#define highlighting_keywords_MARKDOWN		EMPTY_KEYWORDS
#define highlighting_properties_MARKDOWN	EMPTY_PROPERTIES


/* Matlab */
#define highlighting_lexer_MATLAB		SCLEX_OCTAVE /* not MATLAB to support Octave's # comments */
static const HLStyle highlighting_styles_MATLAB[] =
{
	{ SCE_MATLAB_DEFAULT,			"default",				FALSE,	FALSE },
	{ SCE_MATLAB_COMMENT,			"comment",				FALSE,	FALSE },
	{ SCE_MATLAB_COMMAND,			"command",				FALSE,	FALSE },
	{ SCE_MATLAB_NUMBER,			"number",				FALSE,	FALSE },
	{ SCE_MATLAB_KEYWORD,			"keyword",				FALSE,	FALSE },
	{ SCE_MATLAB_STRING,			"string",				FALSE,	FALSE },
	{ SCE_MATLAB_OPERATOR,			"operator",				FALSE,	FALSE },
	{ SCE_MATLAB_IDENTIFIER,		"identifier",			FALSE,	FALSE },
	{ SCE_MATLAB_DOUBLEQUOTESTRING,	"doublequotedstring",	FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_MATLAB[] =
{
	{ 0, "primary",	FALSE }
};
#define highlighting_properties_MATLAB	EMPTY_PROPERTIES


/* NSIS */
#define highlighting_lexer_NSIS			SCLEX_NSIS
static const HLStyle highlighting_styles_NSIS[] =
{
	{ SCE_NSIS_DEFAULT,			"default",			FALSE,	FALSE },
	{ SCE_NSIS_COMMENT,			"comment",			FALSE,	FALSE },
	{ SCE_NSIS_STRINGDQ,		"stringdq",			FALSE,	FALSE },
	{ SCE_NSIS_STRINGLQ,		"stringlq",			FALSE,	FALSE },
	{ SCE_NSIS_STRINGRQ,		"stringrq",			FALSE,	FALSE },
	{ SCE_NSIS_FUNCTION,		"function",			FALSE,	FALSE },
	{ SCE_NSIS_VARIABLE,		"variable",			FALSE,	FALSE },
	{ SCE_NSIS_LABEL,			"label",			FALSE,	FALSE },
	{ SCE_NSIS_USERDEFINED,		"userdefined",		FALSE,	FALSE },
	{ SCE_NSIS_SECTIONDEF,		"sectiondef",		FALSE,	FALSE },
	{ SCE_NSIS_SUBSECTIONDEF,	"subsectiondef",	FALSE,	FALSE },
	{ SCE_NSIS_IFDEFINEDEF,		"ifdefinedef",		FALSE,	FALSE },
	{ SCE_NSIS_MACRODEF,		"macrodef",			FALSE,	FALSE },
	{ SCE_NSIS_STRINGVAR,		"stringvar",		FALSE,	FALSE },
	{ SCE_NSIS_NUMBER,			"number",			FALSE,	FALSE },
	{ SCE_NSIS_SECTIONGROUP,	"sectiongroup",		FALSE,	FALSE },
	{ SCE_NSIS_PAGEEX,			"pageex",			FALSE,	FALSE },
	{ SCE_NSIS_FUNCTIONDEF,		"functiondef",		FALSE,	FALSE },
	{ SCE_NSIS_COMMENTBOX,		"commentbox",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_NSIS[] =
{
	{ 0, "functions",	FALSE },
	{ 1, "variables",	FALSE },
	{ 2, "lables",		FALSE },
	{ 3, "userdefined",	FALSE }
};
#define highlighting_properties_NSIS	EMPTY_PROPERTIES


/* Objective-C */
#define highlighting_lexer_OBJECTIVEC		highlighting_lexer_C
#define highlighting_styles_OBJECTIVEC		highlighting_styles_C
static const HLKeyword highlighting_keywords_OBJECTIVEC[] =
{
	{ 0, "primary",		FALSE },
	/* SCI_SETKEYWORDS = 1 - secondary + global tags file types, see below */
	{ 1, "secondary",	TRUE },
	{ 2, "docComment",	FALSE }
	/* SCI_SETKEYWORDS = 3 is for current session types - see document_highlight_tags() */
};
#define highlighting_properties_OBJECTIVEC	highlighting_properties_C


/* Pascal */
#define highlighting_lexer_PASCAL		SCLEX_PASCAL
static const HLStyle highlighting_styles_PASCAL[] =
{
	{ SCE_PAS_DEFAULT,			"default",			FALSE,	FALSE },
	{ SCE_PAS_IDENTIFIER,		"identifier",		FALSE,	FALSE },
	{ SCE_PAS_COMMENT,			"comment",			FALSE,	FALSE },
	{ SCE_PAS_COMMENT2,			"comment2",			FALSE,	FALSE },
	{ SCE_PAS_COMMENTLINE,		"commentline",		FALSE,	FALSE },
	{ SCE_PAS_PREPROCESSOR,		"preprocessor",		FALSE,	FALSE },
	{ SCE_PAS_PREPROCESSOR2,	"preprocessor2",	FALSE,	FALSE },
	{ SCE_PAS_NUMBER,			"number",			FALSE,	FALSE },
	{ SCE_PAS_HEXNUMBER,		"hexnumber",		FALSE,	FALSE },
	{ SCE_PAS_WORD,				"word",				FALSE,	FALSE },
	{ SCE_PAS_STRING,			"string",			FALSE,	FALSE },
	{ SCE_PAS_STRINGEOL,		"stringeol",		FALSE,	FALSE },
	{ SCE_PAS_CHARACTER,		"character",		FALSE,	FALSE },
	{ SCE_PAS_OPERATOR,			"operator",			FALSE,	FALSE },
	{ SCE_PAS_ASM,				"asm",				FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_PASCAL[] =
{
	{ 0, "primary",	FALSE }
};
#define highlighting_properties_PASCAL	EMPTY_PROPERTIES


/* Perl */
#define highlighting_lexer_PERL			SCLEX_PERL
static const HLStyle highlighting_styles_PERL[] =
{
	{ SCE_PL_DEFAULT,			"default",				FALSE,	FALSE },
	{ SCE_PL_ERROR,				"error",				FALSE,	FALSE },
	{ SCE_PL_COMMENTLINE,		"commentline",			FALSE,	FALSE },
	{ SCE_PL_NUMBER,			"number",				FALSE,	FALSE },
	{ SCE_PL_WORD,				"word",					FALSE,	FALSE },
	{ SCE_PL_STRING,			"string",				FALSE,	FALSE },
	{ SCE_PL_CHARACTER,			"character",			FALSE,	FALSE },
	{ SCE_PL_PREPROCESSOR,		"preprocessor",			FALSE,	FALSE },
	{ SCE_PL_OPERATOR,			"operator",				FALSE,	FALSE },
	{ SCE_PL_IDENTIFIER,		"identifier",			FALSE,	FALSE },
	{ SCE_PL_SCALAR,			"scalar",				FALSE,	FALSE },
	{ SCE_PL_POD,				"pod",					FALSE,	FALSE },
	{ SCE_PL_REGEX,				"regex",				FALSE,	FALSE },
	{ SCE_PL_ARRAY,				"array",				FALSE,	FALSE },
	{ SCE_PL_HASH,				"hash",					FALSE,	FALSE },
	{ SCE_PL_SYMBOLTABLE,		"symboltable",			FALSE,	FALSE },
	{ SCE_PL_BACKTICKS,			"backticks",			FALSE,	FALSE },
	{ SCE_PL_POD_VERB,			"pod_verbatim",			FALSE,	FALSE },
	{ SCE_PL_REGSUBST,			"reg_subst",			FALSE,	FALSE },
	{ SCE_PL_DATASECTION,		"datasection",			FALSE,	FALSE },
	{ SCE_PL_HERE_DELIM,		"here_delim",			FALSE,	FALSE },
	{ SCE_PL_HERE_Q,			"here_q",				FALSE,	FALSE },
	{ SCE_PL_HERE_QQ,			"here_qq",				FALSE,	FALSE },
	{ SCE_PL_HERE_QX,			"here_qx",				FALSE,	FALSE },
	{ SCE_PL_STRING_Q,			"string_q",				FALSE,	FALSE },
	{ SCE_PL_STRING_QQ,			"string_qq",			FALSE,	FALSE },
	{ SCE_PL_STRING_QX,			"string_qx",			FALSE,	FALSE },
	{ SCE_PL_STRING_QR,			"string_qr",			FALSE,	FALSE },
	{ SCE_PL_STRING_QW,			"string_qw",			FALSE,	FALSE },
	{ SCE_PL_VARIABLE_INDEXER,	"variable_indexer",		FALSE,	FALSE },
	{ SCE_PL_PUNCTUATION,		"punctuation",			FALSE,	FALSE },
	{ SCE_PL_LONGQUOTE,			"longquote",			FALSE,	FALSE },
	{ SCE_PL_SUB_PROTOTYPE,		"sub_prototype",		FALSE,	FALSE },
	{ SCE_PL_FORMAT_IDENT,		"format_ident",			FALSE,	FALSE },
	{ SCE_PL_FORMAT,			"format",				FALSE,	FALSE },
	{ SCE_PL_STRING_VAR,		"string_var",			FALSE,	FALSE },
	{ SCE_PL_XLAT,				"xlat",					FALSE,	FALSE },
	{ SCE_PL_REGEX_VAR,			"regex_var",			FALSE,	FALSE },
	{ SCE_PL_REGSUBST_VAR,		"regsubst_var",			FALSE,	FALSE },
	{ SCE_PL_BACKTICKS_VAR,		"backticks_var",		FALSE,	FALSE },
	{ SCE_PL_HERE_QQ_VAR,		"here_qq_var",			FALSE,	FALSE },
	{ SCE_PL_HERE_QX_VAR,		"here_qx_var",			FALSE,	FALSE },
	{ SCE_PL_STRING_QQ_VAR,		"string_qq_var",		FALSE,	FALSE },
	{ SCE_PL_STRING_QX_VAR,		"string_qx_var",		FALSE,	FALSE },
	{ SCE_PL_STRING_QR_VAR,		"string_qr_var",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_PERL[] =
{
	{ 0, "primary",	FALSE }
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
	{ SCE_PO_DEFAULT,				"default",				FALSE,	FALSE },
	{ SCE_PO_COMMENT,				"comment",				FALSE,	FALSE },
	{ SCE_PO_PROGRAMMER_COMMENT,	"programmer_comment",	FALSE,	FALSE },
	{ SCE_PO_REFERENCE,				"reference",			FALSE,	FALSE },
	{ SCE_PO_FLAGS,					"flags",				FALSE,	FALSE },
	{ SCE_PO_FUZZY,					"fuzzy",				FALSE,	FALSE },
	{ SCE_PO_MSGID,					"msgid",				FALSE,	FALSE },
	{ SCE_PO_MSGID_TEXT,			"msgid_text",			FALSE,	FALSE },
	{ SCE_PO_MSGID_TEXT_EOL,		"msgid_text_eol",		FALSE,	FALSE },
	{ SCE_PO_MSGSTR,				"msgstr",				FALSE,	FALSE },
	{ SCE_PO_MSGSTR_TEXT,			"msgstr_text",			FALSE,	FALSE },
	{ SCE_PO_MSGSTR_TEXT_EOL,		"msgstr_text_eol",		FALSE,	FALSE },
	{ SCE_PO_MSGCTXT,				"msgctxt",				FALSE,	FALSE },
	{ SCE_PO_MSGCTXT_TEXT,			"msgctxt_text",			FALSE,	FALSE },
	{ SCE_PO_MSGCTXT_TEXT_EOL,		"msgctxt_text_eol",		FALSE,	FALSE },
	{ SCE_PO_ERROR,					"error",				FALSE,	FALSE }
};
#define highlighting_keywords_PO	EMPTY_KEYWORDS
#define highlighting_properties_PO	EMPTY_PROPERTIES


/* PowerShell */
#define highlighting_lexer_POWERSHELL		SCLEX_POWERSHELL
static const HLStyle highlighting_styles_POWERSHELL[] =
{
	{ SCE_POWERSHELL_DEFAULT,			"default",				FALSE,	FALSE },
	{ SCE_POWERSHELL_COMMENT,			"comment",				FALSE,	FALSE },
	{ SCE_POWERSHELL_STRING,			"string",				FALSE,	FALSE },
	{ SCE_POWERSHELL_CHARACTER,			"character",			FALSE,	FALSE },
	{ SCE_POWERSHELL_NUMBER,			"number",				FALSE,	FALSE },
	{ SCE_POWERSHELL_VARIABLE,			"variable",				FALSE,	FALSE },
	{ SCE_POWERSHELL_OPERATOR,			"operator",				FALSE,	FALSE },
	{ SCE_POWERSHELL_IDENTIFIER,		"identifier",			FALSE,	FALSE },
	{ SCE_POWERSHELL_KEYWORD,			"keyword",				FALSE,	FALSE },
	{ SCE_POWERSHELL_CMDLET,			"cmdlet",				FALSE,	FALSE },
	{ SCE_POWERSHELL_ALIAS,				"alias",				FALSE,	FALSE },
	{ SCE_POWERSHELL_FUNCTION,			"function",				FALSE,	FALSE },
	{ SCE_POWERSHELL_USER1,				"user1",				FALSE,	FALSE },
	{ SCE_POWERSHELL_COMMENTSTREAM,		"commentstream",		FALSE,	FALSE },
	{ SCE_POWERSHELL_HERE_STRING,		"here_string",			FALSE,	FALSE },
	{ SCE_POWERSHELL_HERE_CHARACTER,	"here_character",		FALSE,	FALSE },
	{ SCE_POWERSHELL_COMMENTDOCKEYWORD,	"commentdockeyword",	FALSE,	FALSE },
};
static const HLKeyword highlighting_keywords_POWERSHELL[] =
{
	{ 0, "keywords",	FALSE },
	{ 1, "cmdlets",		FALSE },
	{ 2, "aliases",		FALSE },
	{ 3, "functions",	FALSE },
	{ 4, "user1",		FALSE },
	{ 5, "docComment",	FALSE },
};
#define highlighting_properties_POWERSHELL	EMPTY_PROPERTIES


/* Python */
#define highlighting_lexer_PYTHON		SCLEX_PYTHON
static const HLStyle highlighting_styles_PYTHON[] =
{
	{ SCE_P_DEFAULT,		"default",			FALSE,	FALSE },
	{ SCE_P_COMMENTLINE,	"commentline",		FALSE,	FALSE },
	{ SCE_P_NUMBER,			"number",			FALSE,	FALSE },
	{ SCE_P_STRING,			"string",			FALSE,	FALSE },
	{ SCE_P_CHARACTER,		"character",		FALSE,	FALSE },
	{ SCE_P_WORD,			"word",				FALSE,	FALSE },
	{ SCE_P_TRIPLE,			"triple",			FALSE,	FALSE },
	{ SCE_P_TRIPLEDOUBLE,	"tripledouble",		FALSE,	FALSE },
	{ SCE_P_CLASSNAME,		"classname",		FALSE,	FALSE },
	{ SCE_P_DEFNAME,		"defname",			FALSE,	FALSE },
	{ SCE_P_OPERATOR,		"operator",			FALSE,	FALSE },
	{ SCE_P_IDENTIFIER,		"identifier",		FALSE,	TRUE },
	{ SCE_P_COMMENTBLOCK,	"commentblock",		FALSE,	FALSE },
	{ SCE_P_STRINGEOL,		"stringeol",		FALSE,	FALSE },
	{ SCE_P_WORD2,			"word2",			FALSE,	FALSE },
	{ SCE_P_FSTRING,		"fstring",			FALSE,	FALSE },
	{ SCE_P_FCHARACTER,		"fcharacter",		FALSE,	FALSE },
	{ SCE_P_FTRIPLE,		"ftriple",			FALSE,	FALSE },
	{ SCE_P_FTRIPLEDOUBLE,	"ftripledouble",	FALSE,	FALSE },
	{ SCE_P_DECORATOR,		"decorator",		FALSE,	FALSE },
	{ SCE_P_ATTRIBUTE,		"attribute",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_PYTHON[] =
{
	{ 0, "primary",		FALSE },
	{ 1, "identifiers",	FALSE }
};
#define highlighting_properties_PYTHON	EMPTY_PROPERTIES


/* R */
#define highlighting_lexer_R		SCLEX_R
static const HLStyle highlighting_styles_R[] =
{
	{ SCE_R_DEFAULT,		"default",			FALSE,	FALSE },
	{ SCE_R_COMMENT,		"comment",			FALSE,	FALSE },
	{ SCE_R_KWORD,			"kword",			FALSE,	FALSE },
	{ SCE_R_OPERATOR,		"operator",			FALSE,	FALSE },
	{ SCE_R_BASEKWORD,		"basekword",		FALSE,	FALSE },
	{ SCE_R_OTHERKWORD,		"otherkword",		FALSE,	FALSE },
	{ SCE_R_NUMBER,			"number",			FALSE,	FALSE },
	{ SCE_R_STRING,			"string",			FALSE,	FALSE },
	{ SCE_R_STRING2,		"string2",			FALSE,	FALSE },
	{ SCE_R_IDENTIFIER,		"identifier",		FALSE,	FALSE },
	{ SCE_R_INFIX,			"infix",			FALSE,	FALSE },
	{ SCE_R_INFIXEOL,		"infixeol",			FALSE,	FALSE },
	{ SCE_R_BACKTICKS,		"backticks",		FALSE,	FALSE },
	{ SCE_R_RAWSTRING,		"stringraw",		FALSE,	FALSE },
	{ SCE_R_RAWSTRING2,		"stringraw",		FALSE,	FALSE },
	{ SCE_R_ESCAPESEQUENCE,	"escapesequence",	FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_R[] =
{
	{ 0, "primary",			FALSE },
	{ 1, "package",			FALSE },
	{ 2, "package_other",	FALSE }
};
#define highlighting_properties_R	EMPTY_PROPERTIES


/* Ruby */
#define highlighting_lexer_RUBY			SCLEX_RUBY
static const HLStyle highlighting_styles_RUBY[] =
{
	{ SCE_RB_DEFAULT,		"default",			FALSE,	FALSE },
	{ SCE_RB_COMMENTLINE,	"commentline",		FALSE,	FALSE },
	{ SCE_RB_NUMBER,		"number",			FALSE,	FALSE },
	{ SCE_RB_STRING,		"string",			FALSE,	FALSE },
	{ SCE_RB_CHARACTER,		"character",		FALSE,	FALSE },
	{ SCE_RB_WORD,			"word",				FALSE,	FALSE },
	{ SCE_RB_GLOBAL,		"global",			FALSE,	FALSE },
	{ SCE_RB_SYMBOL,		"symbol",			FALSE,	FALSE },
	{ SCE_RB_CLASSNAME,		"classname",		FALSE,	FALSE },
	{ SCE_RB_DEFNAME,		"defname",			FALSE,	FALSE },
	{ SCE_RB_OPERATOR,		"operator",			FALSE,	FALSE },
	{ SCE_RB_IDENTIFIER,	"identifier",		FALSE,	FALSE },
	{ SCE_RB_MODULE_NAME,	"modulename",		FALSE,	FALSE },
	{ SCE_RB_BACKTICKS,		"backticks",		FALSE,	FALSE },
	{ SCE_RB_INSTANCE_VAR,	"instancevar",		FALSE,	FALSE },
	{ SCE_RB_CLASS_VAR,		"classvar",			FALSE,	FALSE },
	{ SCE_RB_DATASECTION,	"datasection",		FALSE,	FALSE },
	{ SCE_RB_HERE_DELIM,	"heredelim",		FALSE,	FALSE },
	{ SCE_RB_WORD_DEMOTED,	"worddemoted",		FALSE,	FALSE },
	{ SCE_RB_STDIN,			"stdin",			FALSE,	FALSE },
	{ SCE_RB_STDOUT,		"stdout",			FALSE,	FALSE },
	{ SCE_RB_STDERR,		"stderr",			FALSE,	FALSE },
	{ SCE_RB_REGEX,			"regex",			FALSE,	FALSE },
	{ SCE_RB_HERE_Q,		"here_q",			FALSE,	FALSE },
	{ SCE_RB_HERE_QQ,		"here_qq",			FALSE,	FALSE },
	{ SCE_RB_HERE_QX,		"here_qx",			FALSE,	FALSE },
	{ SCE_RB_STRING_Q,		"string_q",			FALSE,	FALSE },
	{ SCE_RB_STRING_QQ,		"string_qq",		FALSE,	FALSE },
	{ SCE_RB_STRING_QX,		"string_qx",		FALSE,	FALSE },
	{ SCE_RB_STRING_QR,		"string_qr",		FALSE,	FALSE },
	{ SCE_RB_STRING_QW,		"string_qw",		FALSE,	FALSE },
	{ SCE_RB_STRING_W,		"string_qw",		FALSE,	FALSE },
	{ SCE_RB_STRING_QI,		"symbol",			FALSE,	FALSE },
	{ SCE_RB_STRING_QS,		"symbol",			FALSE,	FALSE },
	{ SCE_RB_STRING_I,		"symbol",			FALSE,	FALSE },
	{ SCE_RB_UPPER_BOUND,	"upper_bound",		FALSE,	FALSE },
	{ SCE_RB_ERROR,			"error",			FALSE,	FALSE },
	{ SCE_RB_POD,			"pod",				FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_RUBY[] =
{
	{ 0, "primary",	FALSE }
};
#define highlighting_properties_RUBY	EMPTY_PROPERTIES

/* Rust */
#define highlighting_lexer_RUST		SCLEX_RUST
static const HLStyle highlighting_styles_RUST[] =
{
	{ SCE_RUST_DEFAULT,				"default",					FALSE,	FALSE },
	{ SCE_RUST_COMMENTBLOCK,		"commentblock",				FALSE,	FALSE },
	{ SCE_RUST_COMMENTLINE,			"commentline",				FALSE,	FALSE },
	{ SCE_RUST_COMMENTBLOCKDOC,		"commentblockdoc",			FALSE,	FALSE },
	{ SCE_RUST_COMMENTLINEDOC,		"commentlinedoc",			FALSE,	FALSE },
	{ SCE_RUST_NUMBER,				"number",					FALSE,	FALSE },
	{ SCE_RUST_WORD,				"word",						FALSE,	FALSE },
	{ SCE_RUST_WORD2,				"word2",					FALSE,	FALSE },
	{ SCE_RUST_WORD3,				"word3",					FALSE,	FALSE },
	{ SCE_RUST_WORD4,				"word4",					FALSE,	FALSE },
	{ SCE_RUST_WORD5,				"word5",					FALSE,	FALSE },
	{ SCE_RUST_WORD6,				"word6",					FALSE,	FALSE },
	{ SCE_RUST_WORD7,				"word7",					FALSE,	FALSE },
	{ SCE_RUST_STRING,				"string",					FALSE,	FALSE },
	{ SCE_RUST_STRINGR,				"stringraw",				FALSE,	FALSE },
	{ SCE_RUST_CHARACTER,			"character",				FALSE,	FALSE },
	{ SCE_RUST_OPERATOR,			"operator",					FALSE,	FALSE },
	{ SCE_RUST_IDENTIFIER,			"identifier",				FALSE,	FALSE },
	{ SCE_RUST_LIFETIME,			"lifetime",					FALSE,	FALSE },
	{ SCE_RUST_MACRO,				"macro",					FALSE,	FALSE },
	{ SCE_RUST_LEXERROR,			"lexerror",					FALSE,	FALSE },
	{ SCE_RUST_BYTESTRING,			"bytestring",				FALSE,	FALSE },
	{ SCE_RUST_BYTESTRINGR,			"bytestringr",				FALSE,	FALSE },
	{ SCE_RUST_BYTECHARACTER,		"bytecharacter",			FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_RUST[] =
{
	{ 0, "primary",		FALSE },
	/* SCI_SETKEYWORDS = 1 - secondary + global tags file types */
	{ 1, "secondary",	TRUE },
	{ 2, "tertiary",	FALSE },
	/* SCI_SETKEYWORDS = 3 is for current session types - see document_highlight_tags() */
};
#define highlighting_properties_RUST		EMPTY_PROPERTIES

/* SH */
#define highlighting_lexer_SH		SCLEX_BASH
static const HLStyle highlighting_styles_SH[] =
{
	{ SCE_SH_DEFAULT,		"default",		FALSE,	FALSE },
	{ SCE_SH_COMMENTLINE,	"commentline",	FALSE,	FALSE },
	{ SCE_SH_NUMBER,		"number",		FALSE,	FALSE },
	{ SCE_SH_WORD,			"word",			FALSE,	FALSE },
	{ SCE_SH_STRING,		"string",		FALSE,	FALSE },
	{ SCE_SH_CHARACTER,		"character",	FALSE,	FALSE },
	{ SCE_SH_OPERATOR,		"operator",		FALSE,	FALSE },
	{ SCE_SH_IDENTIFIER,	"identifier",	FALSE,	TRUE },
	{ SCE_SH_BACKTICKS,		"backticks",	FALSE,	FALSE },
	{ SCE_SH_PARAM,			"param",		FALSE,	FALSE },
	{ SCE_SH_SCALAR,		"scalar",		FALSE,	TRUE },
	{ SCE_SH_ERROR,			"error",		FALSE,	FALSE },
	{ SCE_SH_HERE_DELIM,	"here_delim",	FALSE,	FALSE },
	{ SCE_SH_HERE_Q,		"here_q",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_SH[] =
{
	{ 0, "primary",	FALSE }
};
#define highlighting_properties_SH	EMPTY_PROPERTIES


/* SMALLTALK */
#define highlighting_lexer_SMALLTALK SCLEX_SMALLTALK
static const HLStyle highlighting_styles_SMALLTALK[] =
{
	{ SCE_ST_DEFAULT,		"default",				FALSE,	FALSE },
	{ SCE_ST_SPECIAL,		"special",				FALSE,	FALSE },
	{ SCE_ST_SYMBOL,		"symbol",				FALSE,	FALSE },
	{ SCE_ST_ASSIGN,		"assignment",			FALSE,	FALSE },
	{ SCE_ST_RETURN,		"return",				FALSE,	FALSE },
	{ SCE_ST_NUMBER,		"number",				FALSE,	FALSE },
	{ SCE_ST_BINARY,		"binary",				FALSE,	FALSE },
	{ SCE_ST_SPEC_SEL,		"special_selector",		FALSE,	FALSE },
	{ SCE_ST_KWSEND,		"keyword_send",			FALSE,	FALSE },
	{ SCE_ST_GLOBAL,		"global",				FALSE,	FALSE },
	{ SCE_ST_SELF,			"self",					FALSE,	FALSE },
	{ SCE_ST_SUPER,			"super",				FALSE,	FALSE },
	{ SCE_ST_NIL,			"nil",					FALSE,	FALSE },
	{ SCE_ST_BOOL,			"bool",					FALSE,	FALSE },
	{ SCE_ST_COMMENT,		"comment",				FALSE,	FALSE },
	{ SCE_ST_STRING,		"string",				FALSE,	FALSE },
	{ SCE_ST_CHARACTER,		"character",			FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_SMALLTALK[] =
{
	{ 0, "special_selector",	FALSE }
};
#define highlighting_properties_SMALLTALK	EMPTY_PROPERTIES


/* SQL */
#define highlighting_lexer_SQL			SCLEX_SQL
static const HLStyle highlighting_styles_SQL[] =
{
	{ SCE_SQL_DEFAULT,					"default",					FALSE,	FALSE },
	{ SCE_SQL_COMMENT,					"comment",					FALSE,	FALSE },
	{ SCE_SQL_COMMENTLINE,				"commentline",				FALSE,	FALSE },
	{ SCE_SQL_COMMENTDOC,				"commentdoc",				FALSE,	FALSE },
	{ SCE_SQL_COMMENTLINEDOC,			"commentlinedoc",			FALSE,	FALSE },
	{ SCE_SQL_COMMENTDOCKEYWORD,		"commentdockeyword",		FALSE,	FALSE },
	{ SCE_SQL_COMMENTDOCKEYWORDERROR,	"commentdockeyworderror",	FALSE,	FALSE },
	{ SCE_SQL_NUMBER,					"number",					FALSE,	FALSE },
	{ SCE_SQL_WORD,						"word",						FALSE,	FALSE },
	{ SCE_SQL_WORD2,					"word2",					FALSE,	FALSE },
	{ SCE_SQL_STRING,					"string",					FALSE,	FALSE },
	{ SCE_SQL_CHARACTER,				"character",				FALSE,	FALSE },
	{ SCE_SQL_OPERATOR,					"operator",					FALSE,	FALSE },
	{ SCE_SQL_IDENTIFIER,				"identifier",				FALSE,	FALSE },
	{ SCE_SQL_SQLPLUS,					"sqlplus",					FALSE,	FALSE },
	{ SCE_SQL_SQLPLUS_PROMPT,			"sqlplus_prompt",			FALSE,	FALSE },
	{ SCE_SQL_SQLPLUS_COMMENT,			"sqlplus_comment",			FALSE,	FALSE },
	{ SCE_SQL_QUOTEDIDENTIFIER,			"quotedidentifier",			FALSE,	FALSE },
	{ SCE_SQL_QOPERATOR,				"qoperator",				FALSE,	FALSE }
	/* these are for user-defined keywords we don't set yet */
	/*{ SCE_SQL_USER1,					"user1",					FALSE,	FALSE },
	{ SCE_SQL_USER2,					"user2",					FALSE,	FALSE },
	{ SCE_SQL_USER3,					"user3",					FALSE,	FALSE },
	{ SCE_SQL_USER4,					"user4",					FALSE,	FALSE }*/
};
static const HLKeyword highlighting_keywords_SQL[] =
{
	{ 0, "keywords",	FALSE }
};
#define highlighting_properties_SQL		EMPTY_PROPERTIES


/* TCL */
#define highlighting_lexer_TCL			SCLEX_TCL
static const HLStyle highlighting_styles_TCL[] =
{
	{ SCE_TCL_DEFAULT,			"default",			FALSE,	FALSE },
	{ SCE_TCL_COMMENT,			"comment",			FALSE,	FALSE },
	{ SCE_TCL_COMMENTLINE,		"commentline",		FALSE,	FALSE },
	{ SCE_TCL_NUMBER,			"number",			FALSE,	FALSE },
	{ SCE_TCL_OPERATOR,			"operator",			FALSE,	FALSE },
	{ SCE_TCL_IDENTIFIER,		"identifier",		FALSE,	FALSE },
	{ SCE_TCL_WORD_IN_QUOTE,	"wordinquote",		FALSE,	FALSE },
	{ SCE_TCL_IN_QUOTE,			"inquote",			FALSE,	FALSE },
	{ SCE_TCL_SUBSTITUTION,		"substitution",		FALSE,	FALSE },
	{ SCE_TCL_MODIFIER,			"modifier",			FALSE,	FALSE },
	{ SCE_TCL_EXPAND,			"expand",			FALSE,	FALSE },
	{ SCE_TCL_WORD,				"wordtcl",			FALSE,	FALSE },
	{ SCE_TCL_WORD2,			"wordtk",			FALSE,	FALSE },
	{ SCE_TCL_WORD3,			"worditcl",			FALSE,	FALSE },
	{ SCE_TCL_WORD4,			"wordtkcmds",		FALSE,	FALSE },
	{ SCE_TCL_WORD5,			"wordexpand",		FALSE,	FALSE },
	{ SCE_TCL_COMMENT_BOX,		"commentbox",		FALSE,	FALSE },
	{ SCE_TCL_BLOCK_COMMENT,	"blockcomment",		FALSE,	FALSE },
	{ SCE_TCL_SUB_BRACE,		"subbrace",			FALSE,	FALSE }
	/* these are for user-defined keywords we don't set yet */
	/*{ SCE_TCL_WORD6,			"user2",			FALSE,	FALSE },
	{ SCE_TCL_WORD7,			"user3",			FALSE,	FALSE },
	{ SCE_TCL_WORD8,			"user4",			FALSE,	FALSE }*/
};
static const HLKeyword highlighting_keywords_TCL[] =
{
	{ 0, "tcl",			FALSE },
	{ 1, "tk",			FALSE },
	{ 2, "itcl",		FALSE },
	{ 3, "tkcommands",	FALSE },
	{ 4, "expand",		FALSE }
};
#define highlighting_properties_TCL		EMPTY_PROPERTIES


/* Txt2Tags */
#define highlighting_lexer_TXT2TAGS			SCLEX_TXT2TAGS
static const HLStyle highlighting_styles_TXT2TAGS[] =
{
	{ SCE_TXT2TAGS_DEFAULT,		"default",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_LINE_BEGIN,	"default",		FALSE,	FALSE }, /* XIFME: remappings should be avoided */
	{ SCE_TXT2TAGS_PRECHAR,		"default",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_STRONG1,		"strong",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_STRONG2,		"strong",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_EM1,			"emphasis",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_EM2,			"underlined",	FALSE,	FALSE }, /* WTF? */
	{ SCE_TXT2TAGS_HEADER1,		"header1",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_HEADER2,		"header2",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_HEADER3,		"header3",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_HEADER4,		"header4",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_HEADER5,		"header5",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_HEADER6,		"header6",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_ULIST_ITEM,	"ulist_item",	FALSE,	FALSE },
	{ SCE_TXT2TAGS_OLIST_ITEM,	"olist_item",	FALSE,	FALSE },
	{ SCE_TXT2TAGS_BLOCKQUOTE,	"blockquote",	FALSE,	FALSE },
	{ SCE_TXT2TAGS_STRIKEOUT,	"strikeout",	FALSE,	FALSE },
	{ SCE_TXT2TAGS_HRULE,		"hrule",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_LINK,		"link",			FALSE,	FALSE },
	{ SCE_TXT2TAGS_CODE,		"code",			FALSE,	FALSE },
	{ SCE_TXT2TAGS_CODE2,		"code",			FALSE,	FALSE },
	{ SCE_TXT2TAGS_CODEBK,		"codebk",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_COMMENT,		"comment",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_OPTION,		"option",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_PREPROC,		"preproc",		FALSE,	FALSE },
	{ SCE_TXT2TAGS_POSTPROC,	"postproc",		FALSE,	FALSE }
};
#define highlighting_keywords_TXT2TAGS		EMPTY_KEYWORDS
#define highlighting_properties_TXT2TAGS	EMPTY_PROPERTIES


/* VHDL */
#define highlighting_lexer_VHDL			SCLEX_VHDL
static const HLStyle highlighting_styles_VHDL[] =
{
	{ SCE_VHDL_DEFAULT,			"default",				FALSE,	FALSE },
	{ SCE_VHDL_COMMENT,			"comment",				FALSE,	FALSE },
	{ SCE_VHDL_COMMENTLINEBANG,	"comment_line_bang",	FALSE,	FALSE },
	{ SCE_VHDL_BLOCK_COMMENT,	"block_comment",		FALSE,	FALSE },
	{ SCE_VHDL_NUMBER,			"number",				FALSE,	FALSE },
	{ SCE_VHDL_STRING,			"string",				FALSE,	FALSE },
	{ SCE_VHDL_OPERATOR,		"operator",				FALSE,	FALSE },
	{ SCE_VHDL_IDENTIFIER,		"identifier",			FALSE,	FALSE },
	{ SCE_VHDL_STRINGEOL,		"stringeol",			FALSE,	FALSE },
	{ SCE_VHDL_KEYWORD,			"keyword",				FALSE,	FALSE },
	{ SCE_VHDL_STDOPERATOR,		"stdoperator",			FALSE,	FALSE },
	{ SCE_VHDL_ATTRIBUTE,		"attribute",			FALSE,	FALSE },
	{ SCE_VHDL_STDFUNCTION,		"stdfunction",			FALSE,	FALSE },
	{ SCE_VHDL_STDPACKAGE,		"stdpackage",			FALSE,	FALSE },
	{ SCE_VHDL_STDTYPE,			"stdtype",				FALSE,	FALSE },
	{ SCE_VHDL_USERWORD,		"userword",				FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_VHDL[] =
{
	{ 0, "keywords",		FALSE },
	{ 1, "operators",		FALSE },
	{ 2, "attributes",		FALSE },
	{ 3, "std_functions",	FALSE },
	{ 4, "std_packages",	FALSE },
	{ 5, "std_types",		FALSE },
	{ 6, "userwords",		FALSE },
};
#define highlighting_properties_VHDL	EMPTY_PROPERTIES


/* Verilog */
#define highlighting_lexer_VERILOG			SCLEX_VERILOG
static const HLStyle highlighting_styles_VERILOG[] =
{
	{ SCE_V_DEFAULT,			"default",				FALSE,	FALSE },
	{ SCE_V_COMMENT,			"comment",				FALSE,	FALSE },
	{ SCE_V_COMMENTLINE,		"comment_line",			FALSE,	FALSE },
	{ SCE_V_COMMENTLINEBANG,	"comment_line_bang",	FALSE,	FALSE },
	{ SCE_V_NUMBER,				"number",				FALSE,	FALSE },
	{ SCE_V_WORD,				"word",					FALSE,	FALSE },
	{ SCE_V_STRING,				"string",				FALSE,	FALSE },
	{ SCE_V_WORD2,				"word2",				FALSE,	FALSE },
	{ SCE_V_WORD3,				"word3",				FALSE,	FALSE },
	{ SCE_V_PREPROCESSOR,		"preprocessor",			FALSE,	FALSE },
	{ SCE_V_OPERATOR,			"operator",				FALSE,	FALSE },
	{ SCE_V_IDENTIFIER,			"identifier",			FALSE,	FALSE },
	{ SCE_V_STRINGEOL,			"stringeol",			FALSE,	FALSE },
	{ SCE_V_USER,				"userword",				FALSE,	FALSE },
	{ SCE_V_COMMENT_WORD,		"comment_word",			FALSE,	FALSE },
	{ SCE_V_INPUT,				"input",				FALSE,	FALSE },
	{ SCE_V_OUTPUT,				"output",				FALSE,	FALSE },
	{ SCE_V_INOUT,				"inout",				FALSE,	FALSE },
	{ SCE_V_PORT_CONNECT,		"port_connect",			FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_VERILOG[] =
{
	{ 0, "word",		FALSE },
	{ 1, "word2",		FALSE },
	{ 2, "word3",		FALSE },
	{ 4, "docComment",	FALSE }
};
#define highlighting_properties_VERILOG		EMPTY_PROPERTIES


/* XML */
#define highlighting_lexer_XML			SCLEX_XML
#define highlighting_styles_XML			highlighting_styles_HTML
static const HLKeyword highlighting_keywords_XML[] =
{
	{ 5, "sgml",	FALSE }
};
#define highlighting_properties_XML		highlighting_properties_HTML


/* YAML */
#define highlighting_lexer_YAML			SCLEX_YAML
static const HLStyle highlighting_styles_YAML[] =
{
	{ SCE_YAML_DEFAULT,		"default",		FALSE,	FALSE },
	{ SCE_YAML_COMMENT,		"comment",		FALSE,	FALSE },
	{ SCE_YAML_IDENTIFIER,	"identifier",	FALSE,	FALSE },
	{ SCE_YAML_KEYWORD,		"keyword",		FALSE,	FALSE },
	{ SCE_YAML_NUMBER,		"number",		FALSE,	FALSE },
	{ SCE_YAML_REFERENCE,	"reference",	FALSE,	FALSE },
	{ SCE_YAML_DOCUMENT,	"document",		FALSE,	FALSE },
	{ SCE_YAML_TEXT,		"text",			FALSE,	FALSE },
	{ SCE_YAML_ERROR,		"error",		FALSE,	FALSE },
	{ SCE_YAML_OPERATOR,	"operator",		FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_YAML[] =
{
	{ 0, "keywords",	FALSE }
};
#define highlighting_properties_YAML	EMPTY_PROPERTIES


/* Zephir */
#define highlighting_lexer_ZEPHIR		SCLEX_PHPSCRIPT
#define highlighting_styles_ZEPHIR		highlighting_styles_PHP
#define highlighting_keywords_ZEPHIR	highlighting_keywords_PHP
#define highlighting_properties_ZEPHIR	highlighting_properties_PHP


/* AutoIt */
#define highlighting_lexer_AU3			SCLEX_AU3
static const HLStyle highlighting_styles_AU3[] =
{
	{ SCE_AU3_DEFAULT,		"default",		FALSE,	FALSE },
	{ SCE_AU3_COMMENT,		"comment",		FALSE,	FALSE },
	{ SCE_AU3_COMMENTBLOCK,	"commentblock",	FALSE,	FALSE },
	{ SCE_AU3_NUMBER,		"number",		FALSE,	FALSE },
	{ SCE_AU3_FUNCTION,		"function",		FALSE,	FALSE },
	{ SCE_AU3_KEYWORD,		"keyword",		FALSE,	FALSE },
	{ SCE_AU3_MACRO,		"macro",		FALSE,	FALSE },
	{ SCE_AU3_STRING,		"string",		FALSE,	FALSE },
	{ SCE_AU3_OPERATOR,		"operator",		FALSE,	FALSE },
	{ SCE_AU3_VARIABLE,		"variable",		FALSE,	FALSE },
	{ SCE_AU3_SENT,			"sent",			FALSE,	FALSE },
	{ SCE_AU3_PREPROCESSOR,	"preprocessor",	FALSE,	FALSE },
	{ SCE_AU3_SPECIAL,		"special",		FALSE,	FALSE },
	{ SCE_AU3_EXPAND,		"expand",		FALSE,	FALSE },
	{ SCE_AU3_COMOBJ,		"comobj",		FALSE,	FALSE },
	{ SCE_AU3_UDF,			"udf",			FALSE,	FALSE }
};
static const HLKeyword highlighting_keywords_AU3[] =
{
	{ 0, "keywords",	FALSE },
	{ 1, "functions",	FALSE },
	{ 2, "macros",		FALSE },
	{ 3, "sent",		FALSE },
	{ 4, "preprocessor",	FALSE },
	{ 5, "special",		FALSE },
	{ 6, "expand",		FALSE },
	{ 7, "udf",		FALSE }
};
#define highlighting_properties_AU3		EMPTY_PROPERTIES

G_END_DECLS

#endif /* GEANY_HIGHLIGHTING_MAPPINGS_H */
