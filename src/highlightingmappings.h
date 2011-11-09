/*
 *      highlightingmappings.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2011 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2011 Colomban Wendling <ban(at)herbesfolles(dot)org>
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


#ifndef GEANY_HIGHLIGHTINGMAPPINGS_H
#define GEANY_HIGHLIGHTINGMAPPINGS_H 1

#include "Scintilla.h"


/* contains all filtypes informations in the form of:
 *  - highlighting_lexer_LANG:		the SCI lexer
 *  - highlighting_styles_LANG:		SCI style/named style mappings
 *  - highlighting_keywords_LANG:	keywords ID/name mappings
 *  - highlighting_properties_LANG:	default SCI properties and their value
 * where LANG is the lang part from GEANY_FILETYPE_LANG
 * 
 * Using this scheme makes possible to automate style setup by simply listing LANG
 * and let STYLESET_[INIT_]FROM_MAPPING() macro prepare the correct calls.
 */


/*
 * Note about initializer lists below:
 *
 * Quoting C99 201x draft at 6.7.9§21:
 *
 *   If there are fewer initializers in a brace-enclosed list than there are elements or
 *   members of an aggregate, or fewer characters in a string literal used to initialize
 *   an array of known size than there are elements in the array, the remainder of the
 *   aggregate shall be initialized implicitly the same as objects that have static
 *   storage duration.
 *
 * Which refers to 6.7.9§10:
 *
 *   [...] If an object that has static or thread storage duration is not initialized
 *   explicitly, then:
 *   - if it has pointer type, it is initialized to a null pointer;
 *   - if it has arithmetic type, it is initialized to (positive or unsigned) zero;
 *   - if it is an aggregate, every member is initialized (recursively) according to
 *     these rules, and any padding is initialized to zero bits;
 *   - if it is a union, the first named member is initialized (recursively) according
 *     to these rules, and any padding is initialized to zero bits;
 *
 * We can then safely leave initializing of ending members to the implicit rules if we
 * want them to be 0 (FALSE is 0, of course).  This is used in the initializers below
 * for better readability and smaller efforts.
 */


/* first HLStyle array entry should always be the default style for that lexer */
typedef struct
{
	guint		 style;	/* SCI style */
	const gchar	*name;	/* style name in the filetypes.* file */
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


/* Ada */
#define highlighting_lexer_ADA			SCLEX_ADA
static const HLStyle highlighting_styles_ADA[] =
{
	{ SCE_ADA_DEFAULT,		"default"		 },
	{ SCE_ADA_WORD,			"word"			 },
	{ SCE_ADA_IDENTIFIER,	"identifier"	 },
	{ SCE_ADA_NUMBER,		"number"		 },
	{ SCE_ADA_DELIMITER,	"delimiter"		 },
	{ SCE_ADA_CHARACTER,	"character"		 },
	{ SCE_ADA_CHARACTEREOL,	"charactereol"	 },
	{ SCE_ADA_STRING,		"string"		 },
	{ SCE_ADA_STRINGEOL,	"stringeol"		 },
	{ SCE_ADA_LABEL,		"label"			 },
	{ SCE_ADA_COMMENTLINE,	"commentline"	 },
	{ SCE_ADA_ILLEGAL,		"illegal"		 }
};
static const HLKeyword highlighting_keywords_ADA[] =
{
	{ 0, "primary" }
};
#define highlighting_properties_ADA		EMPTY_PROPERTIES


/* ActionScript */
#define highlighting_lexer_AS		SCLEX_CPP
#define highlighting_styles_AS		highlighting_styles_C
static const HLKeyword highlighting_keywords_AS[] =
{
	{ 0, "primary" },
	{ 1, "secondary" },
	{ 3, "classes" }
};
#define highlighting_properties_AS	highlighting_properties_C


/* ASM */
#define highlighting_lexer_ASM			SCLEX_ASM
static const HLStyle highlighting_styles_ASM[] =
{
	{ SCE_ASM_DEFAULT,			"default"			 },
	{ SCE_ASM_COMMENT,			"comment"			 },
	{ SCE_ASM_NUMBER,			"number"			 },
	{ SCE_ASM_STRING,			"string"			 },
	{ SCE_ASM_OPERATOR,			"operator"			 },
	{ SCE_ASM_IDENTIFIER,		"identifier"		 },
	{ SCE_ASM_CPUINSTRUCTION,	"cpuinstruction"	 },
	{ SCE_ASM_MATHINSTRUCTION,	"mathinstruction"	 },
	{ SCE_ASM_REGISTER,			"register"			 },
	{ SCE_ASM_DIRECTIVE,		"directive"			 },
	{ SCE_ASM_DIRECTIVEOPERAND,	"directiveoperand"	 },
	{ SCE_ASM_COMMENTBLOCK,		"commentblock"		 },
	{ SCE_ASM_CHARACTER,		"character"			 },
	{ SCE_ASM_STRINGEOL,		"stringeol"			 },
	{ SCE_ASM_EXTINSTRUCTION,	"extinstruction"	 },
	{ SCE_ASM_COMMENTDIRECTIVE,	"commentdirective"	 }
};
static const HLKeyword highlighting_keywords_ASM[] =
{
	{ 0, "instructions" },
	/*{ 1, "instructions" },*/
	{ 2, "registers" },
	{ 3, "directives" }
	/*{ 5, "instructions" }*/
};
#define highlighting_properties_ASM		EMPTY_PROPERTIES


/* BASIC */
#define highlighting_lexer_BASIC		SCLEX_FREEBASIC
static const HLStyle highlighting_styles_BASIC[] =
{
	{ SCE_B_DEFAULT,		"default"		 },
	{ SCE_B_COMMENT,		"comment"		 },
	{ SCE_B_NUMBER,			"number"		 },
	{ SCE_B_KEYWORD,		"word"			 },
	{ SCE_B_STRING,			"string"		 },
	{ SCE_B_PREPROCESSOR,	"preprocessor"	 },
	{ SCE_B_OPERATOR,		"operator"		 },
	{ SCE_B_IDENTIFIER,		"identifier"	 },
	{ SCE_B_DATE,			"date"			 },
	{ SCE_B_STRINGEOL,		"stringeol"		 },
	{ SCE_B_KEYWORD2,		"word2"			 },
	{ SCE_B_KEYWORD3,		"word3"			 },
	{ SCE_B_KEYWORD4,		"word4"			 },
	{ SCE_B_CONSTANT,		"constant"		 },
	{ SCE_B_ASM,			"asm"			 },
	{ SCE_B_LABEL,			"label"			 },
	{ SCE_B_ERROR,			"error"			 },
	{ SCE_B_HEXNUMBER,		"hexnumber"		 },
	{ SCE_B_BINNUMBER,		"binnumber"		 }
};
static const HLKeyword highlighting_keywords_BASIC[] =
{
	{ 0, "keywords" },
	{ 1, "preprocessor" },
	{ 2, "user1" },
	{ 3, "user2" }
};
#define highlighting_properties_BASIC	EMPTY_PROPERTIES


/* C */
/* Also used by some other SCLEX_CPP-based filetypes */
#define highlighting_lexer_C		SCLEX_CPP
static const HLStyle highlighting_styles_C[] =
{
	{ SCE_C_DEFAULT,				"default"				 },
	{ SCE_C_COMMENT,				"comment"				 },
	{ SCE_C_COMMENTLINE,			"commentline"			 },
	{ SCE_C_COMMENTDOC,				"commentdoc"			 },
	{ SCE_C_NUMBER,					"number"				 },
	{ SCE_C_WORD,					"word"					 },
	{ SCE_C_WORD2,					"word2"					 },
	{ SCE_C_STRING,					"string"				 },
	{ SCE_C_CHARACTER,				"character"				 },
	{ SCE_C_UUID,					"uuid"					 },
	{ SCE_C_PREPROCESSOR,			"preprocessor"			 },
	{ SCE_C_OPERATOR,				"operator"				 },
	{ SCE_C_IDENTIFIER,				"identifier"			 },
	{ SCE_C_STRINGEOL,				"stringeol"				 },
	{ SCE_C_VERBATIM,				"verbatim"				 },
	/* triple verbatims use the same style */
	{ SCE_C_TRIPLEVERBATIM,			"verbatim"				 },
	{ SCE_C_REGEX,					"regex"					 },
	{ SCE_C_COMMENTLINEDOC,			"commentlinedoc"		 },
	{ SCE_C_COMMENTDOCKEYWORD,		"commentdockeyword"		 },
	{ SCE_C_COMMENTDOCKEYWORDERROR,	"commentdockeyworderror" },
	/* used for local structs and typedefs */
	{ SCE_C_GLOBALCLASS,			"globalclass"			 }
};
static const HLKeyword highlighting_keywords_C[] =
{
	{ 0, "primary" },
	/* SCI_SETKEYWORDS = 1 - secondary + global tags file types, see below */
	{ 1, "secondary", TRUE },
	{ 2, "docComment" }
	/* SCI_SETKEYWORDS = 3 is for current session types - see editor_lexer_get_type_keyword_idx() */
};
static const HLProperty highlighting_properties_C[] =
{
	{ "fold.cpp.comment.explicit", "0" }
};


/* Caml */
#define highlighting_lexer_CAML			SCLEX_CAML
static const HLStyle highlighting_styles_CAML[] =
{
	{ SCE_CAML_DEFAULT,		"default"	 },
	{ SCE_CAML_COMMENT,		"comment"	 },
	{ SCE_CAML_COMMENT1,	"comment1"	 },
	{ SCE_CAML_COMMENT2,	"comment2"	 },
	{ SCE_CAML_COMMENT3,	"comment3"	 },
	{ SCE_CAML_NUMBER,		"number"	 },
	{ SCE_CAML_KEYWORD,		"keyword"	 },
	{ SCE_CAML_KEYWORD2,	"keyword2"	 },
	{ SCE_CAML_KEYWORD3,	"keyword3"	 },
	{ SCE_CAML_STRING,		"string"	 },
	{ SCE_CAML_CHAR,		"char"		 },
	{ SCE_CAML_OPERATOR,	"operator"	 },
	{ SCE_CAML_IDENTIFIER,	"identifier" },
	{ SCE_CAML_TAGNAME,		"tagname"	 },
	{ SCE_CAML_LINENUM,		"linenum"	 },
	{ SCE_CAML_WHITE,		"white"		 }
};
static const HLKeyword highlighting_keywords_CAML[] =
{
	{ 0, "keywords" },
	{ 1, "keywords_optional" }
};
#define highlighting_properties_CAML	EMPTY_PROPERTIES


/* CMake */
#define highlighting_lexer_CMAKE		SCLEX_CMAKE
static const HLStyle highlighting_styles_CMAKE[] =
{
	{ SCE_CMAKE_DEFAULT,		"default"		 },
	{ SCE_CMAKE_COMMENT,		"comment"		 },
	{ SCE_CMAKE_STRINGDQ,		"stringdq"		 },
	{ SCE_CMAKE_STRINGLQ,		"stringlq"		 },
	{ SCE_CMAKE_STRINGRQ,		"stringrq"		 },
	{ SCE_CMAKE_COMMANDS,		"command"		 },
	{ SCE_CMAKE_PARAMETERS,		"parameters"	 },
	{ SCE_CMAKE_VARIABLE,		"variable"		 },
	{ SCE_CMAKE_USERDEFINED,	"userdefined"	 },
	{ SCE_CMAKE_WHILEDEF,		"whiledef"		 },
	{ SCE_CMAKE_FOREACHDEF,		"foreachdef"	 },
	{ SCE_CMAKE_IFDEFINEDEF,	"ifdefinedef"	 },
	{ SCE_CMAKE_MACRODEF,		"macrodef"		 },
	{ SCE_CMAKE_STRINGVAR,		"stringvar"		 },
	{ SCE_CMAKE_NUMBER,			"number"		 }
};
static const HLKeyword highlighting_keywords_CMAKE[] =
{
	{ 0, "commands" },
	{ 1, "parameters" },
	{ 2, "userdefined" }
};
#define highlighting_properties_CMAKE	EMPTY_PROPERTIES


/* CSS */
#define highlighting_lexer_CSS			SCLEX_CSS
static const HLStyle highlighting_styles_CSS[] =
{
	{ SCE_CSS_DEFAULT,					"default"					 },
	{ SCE_CSS_COMMENT,					"comment"					 },
	{ SCE_CSS_TAG,						"tag"						 },
	{ SCE_CSS_CLASS,					"class"						 },
	{ SCE_CSS_PSEUDOCLASS,				"pseudoclass"				 },
	{ SCE_CSS_UNKNOWN_PSEUDOCLASS,		"unknown_pseudoclass"		 },
	{ SCE_CSS_UNKNOWN_IDENTIFIER,		"unknown_identifier"		 },
	{ SCE_CSS_OPERATOR,					"operator"					 },
	{ SCE_CSS_IDENTIFIER,				"identifier"				 },
	{ SCE_CSS_DOUBLESTRING,				"doublestring"				 },
	{ SCE_CSS_SINGLESTRING,				"singlestring"				 },
	{ SCE_CSS_ATTRIBUTE,				"attribute"					 },
	{ SCE_CSS_VALUE,					"value"						 },
	{ SCE_CSS_ID,						"id"						 },
	{ SCE_CSS_IDENTIFIER2,				"identifier2"				 },
	{ SCE_CSS_IMPORTANT,				"important"					 },
	{ SCE_CSS_DIRECTIVE,				"directive"					 },
	{ SCE_CSS_IDENTIFIER3,				"identifier3"				 },
	{ SCE_CSS_PSEUDOELEMENT,			"pseudoelement"				 },
	{ SCE_CSS_EXTENDED_IDENTIFIER,		"extended_identifier"		 },
	{ SCE_CSS_EXTENDED_PSEUDOCLASS,		"extended_pseudoclass"		 },
	{ SCE_CSS_EXTENDED_PSEUDOELEMENT,	"extended_pseudoelement"	 },
	{ SCE_CSS_MEDIA,					"media"						 }
};
static const HLKeyword highlighting_keywords_CSS[] =
{
	{ 0, "primary" },
	{ 1, "pseudoclasses" },
	{ 2, "secondary" },
	{ 3, "css3_properties" },
	{ 4, "pseudo_elements" },
	{ 5, "browser_css_properties" },
	{ 6, "browser_pseudo_classes" },
	{ 7, "browser_pseudo_elements" }
};
#define highlighting_properties_CSS		EMPTY_PROPERTIES


/* Cobol */
#define highlighting_lexer_COBOL		SCLEX_COBOL
#define highlighting_styles_COBOL		highlighting_styles_C
static const HLKeyword highlighting_keywords_COBOL[] =
{
	{ 0, "primary" },
	{ 1, "secondary" },
	{ 2, "extended_keywords" }
};
#define highlighting_properties_COBOL	highlighting_properties_C


/* Conf */
#define highlighting_lexer_CONF			SCLEX_PROPERTIES
static const HLStyle highlighting_styles_CONF[] =
{
	{ SCE_PROPS_DEFAULT,	"default"	 },
	{ SCE_PROPS_COMMENT,	"comment"	 },
	{ SCE_PROPS_SECTION,	"section"	 },
	{ SCE_PROPS_KEY,		"key"		 },
	{ SCE_PROPS_ASSIGNMENT,	"assignment" },
	{ SCE_PROPS_DEFVAL,		"defval"	 }
};
#define highlighting_keywords_CONF		EMPTY_KEYWORDS
#define highlighting_properties_CONF	EMPTY_PROPERTIES


/* D */
#define highlighting_lexer_D		SCLEX_D
static const HLStyle highlighting_styles_D[] =
{
	{ SCE_D_DEFAULT,				"default"				 },
	{ SCE_D_COMMENT,				"comment"				 },
	{ SCE_D_COMMENTLINE,			"commentline"			 },
	{ SCE_D_COMMENTDOC,				"commentdoc"			 },
	{ SCE_D_COMMENTNESTED,			"commentdocnested"		 },
	{ SCE_D_NUMBER,					"number"				 },
	{ SCE_D_WORD,					"word"					 },
	{ SCE_D_WORD2,					"word2"					 },
	{ SCE_D_WORD3,					"word3"					 },
	{ SCE_D_TYPEDEF,				"typedef"				 }, /* FIXME: don't remap here */
	{ SCE_D_WORD5,					"typedef"				 },
	{ SCE_D_STRING,					"string"				 },
	{ SCE_D_STRINGB,				"string"				 },
	{ SCE_D_STRINGR,				"string"				 },
	{ SCE_D_STRINGEOL,				"stringeol"				 },
	{ SCE_D_CHARACTER,				"character"				 },
	{ SCE_D_OPERATOR,				"operator"				 },
	{ SCE_D_IDENTIFIER,				"identifier"			 },
	{ SCE_D_COMMENTLINEDOC,			"commentlinedoc"		 },
	{ SCE_D_COMMENTDOCKEYWORD,		"commentdockeyword"		 },
	{ SCE_D_COMMENTDOCKEYWORDERROR,	"commentdockeyworderror" }
	/* these are for user-defined keywords we don't set yet */
	/*{ SCE_D_WORD6,					"word6"					 },
	{ SCE_D_WORD7,					"word7"					 }*/
};
static const HLKeyword highlighting_keywords_D[] =
{
	{ 0, "primary" },
	/* SCI_SETKEYWORDS = 1 - secondary + global tags file types */
	{ 1, "secondary", TRUE },
	{ 2, "docComment" },
	/* SCI_SETKEYWORDS = 3 is for current session types - see editor_lexer_get_type_keyword_idx() */
	{ 4, "types" },
};
#define highlighting_properties_D		EMPTY_PROPERTIES


/* Diff */
#define highlighting_lexer_DIFF			SCLEX_DIFF
static const HLStyle highlighting_styles_DIFF[] =
{
	{ SCE_DIFF_DEFAULT,		"default"	 },
	{ SCE_DIFF_COMMENT,		"comment"	 },
	{ SCE_DIFF_COMMAND,		"command"	 },
	{ SCE_DIFF_HEADER,		"header"	 },
	{ SCE_DIFF_POSITION,	"position"	 },
	{ SCE_DIFF_DELETED,		"deleted"	 },
	{ SCE_DIFF_ADDED,		"added"		 },
	{ SCE_DIFF_CHANGED,		"changed"	 }
};
#define highlighting_keywords_DIFF		EMPTY_KEYWORDS
#define highlighting_properties_DIFF	EMPTY_PROPERTIES


/* Erlang */
#define highlighting_lexer_ERLANG		SCLEX_ERLANG
static const HLStyle highlighting_styles_ERLANG[] =
{
	{ SCE_ERLANG_DEFAULT,			"default"			 },
	{ SCE_ERLANG_COMMENT,			"comment"			 },
	{ SCE_ERLANG_VARIABLE,			"variable"			 },
	{ SCE_ERLANG_NUMBER,			"number"			 },
	{ SCE_ERLANG_KEYWORD,			"keyword"			 },
	{ SCE_ERLANG_STRING,			"string"			 },
	{ SCE_ERLANG_OPERATOR,			"operator"			 },
	{ SCE_ERLANG_ATOM,				"atom"				 },
	{ SCE_ERLANG_FUNCTION_NAME,		"function_name"		 },
	{ SCE_ERLANG_CHARACTER,			"character"			 },
	{ SCE_ERLANG_MACRO,				"macro"				 },
	{ SCE_ERLANG_RECORD,			"record"			 },
	{ SCE_ERLANG_PREPROC,			"preproc"			 },
	{ SCE_ERLANG_NODE_NAME,			"node_name"			 },
	{ SCE_ERLANG_COMMENT_FUNCTION,	"comment_function"	 },
	{ SCE_ERLANG_COMMENT_MODULE,	"comment_module"	 },
	{ SCE_ERLANG_COMMENT_DOC,		"comment_doc"		 },
	{ SCE_ERLANG_COMMENT_DOC_MACRO,	"comment_doc_macro"	 },
	{ SCE_ERLANG_ATOM_QUOTED,		"atom_quoted"		 },
	{ SCE_ERLANG_MACRO_QUOTED,		"macro_quoted"		 },
	{ SCE_ERLANG_RECORD_QUOTED,		"record_quoted"		 },
	{ SCE_ERLANG_NODE_NAME_QUOTED,	"node_name_quoted"	 },
	{ SCE_ERLANG_BIFS,				"bifs"				 },
	{ SCE_ERLANG_MODULES,			"modules"			 },
	{ SCE_ERLANG_MODULES_ATT,		"modules_att"		 },
	{ SCE_ERLANG_UNKNOWN,			"unknown"			 }
};
static const HLKeyword highlighting_keywords_ERLANG[] =
{
	{ 0, "keywords" },
	{ 1, "bifs" },
	{ 2, "preproc" },
	{ 3, "module" },
	{ 4, "doc" },
	{ 5, "doc_macro" }
};
#define highlighting_properties_ERLANG	EMPTY_PROPERTIES


/* F77 */
#define highlighting_lexer_F77			SCLEX_F77
static const HLStyle highlighting_styles_F77[] =
{
	{ SCE_F_DEFAULT,		"default"		 },
	{ SCE_F_COMMENT,		"comment"		 },
	{ SCE_F_NUMBER,			"number"		 },
	{ SCE_F_STRING1,		"string"		 },
	{ SCE_F_OPERATOR,		"operator"		 },
	{ SCE_F_IDENTIFIER,		"identifier"	 },
	{ SCE_F_STRING2,		"string2"		 },
	{ SCE_F_WORD,			"word"			 },
	{ SCE_F_WORD2,			"word2"			 },
	{ SCE_F_WORD3,			"word3"			 },
	{ SCE_F_PREPROCESSOR,	"preprocessor"	 },
	{ SCE_F_OPERATOR2,		"operator2"		 },
	{ SCE_F_CONTINUATION,	"continuation"	 },
	{ SCE_F_STRINGEOL,		"stringeol"		 },
	{ SCE_F_LABEL,			"label"			 }
};
static const HLKeyword highlighting_keywords_F77[] =
{
	{ 0, "primary" },
	{ 1, "intrinsic_functions" },
	{ 2, "user_functions" }
};
#define highlighting_properties_F77		EMPTY_PROPERTIES


/* Ferite */
#define highlighting_lexer_FERITE		SCLEX_CPP
#define highlighting_styles_FERITE		highlighting_styles_C
static const HLKeyword highlighting_keywords_FERITE[] =
{
	{ 0, "primary" },
	{ 1, "types" },
	{ 2, "docComment" }
};
#define highlighting_properties_FERITE	highlighting_properties_C


/* Forth */
#define highlighting_lexer_FORTH		SCLEX_FORTH
static const HLStyle highlighting_styles_FORTH[] =
{
	{ SCE_FORTH_DEFAULT,	"default"	 },
	{ SCE_FORTH_COMMENT,	"comment"	 },
	{ SCE_FORTH_COMMENT_ML,	"commentml"	 },
	{ SCE_FORTH_IDENTIFIER,	"identifier" },
	{ SCE_FORTH_CONTROL,	"control"	 },
	{ SCE_FORTH_KEYWORD,	"keyword"	 },
	{ SCE_FORTH_DEFWORD,	"defword"	 },
	{ SCE_FORTH_PREWORD1,	"preword1"	 },
	{ SCE_FORTH_PREWORD2,	"preword2"	 },
	{ SCE_FORTH_NUMBER,		"number"	 },
	{ SCE_FORTH_STRING,		"string"	 },
	{ SCE_FORTH_LOCALE,		"locale"	 }
};
static const HLKeyword highlighting_keywords_FORTH[] =
{
	{ 0, "primary" }
};
#define highlighting_properties_FORTH	EMPTY_PROPERTIES


/* Fortran */
/* F77 and Fortran (F9x) uses different lexers but shares styles and keywords */
#define highlighting_lexer_FORTRAN			SCLEX_FORTRAN
#define highlighting_styles_FORTRAN			highlighting_styles_F77
#define highlighting_keywords_FORTRAN		highlighting_keywords_F77
#define highlighting_properties_FORTRAN		highlighting_properties_F77


/* Haskell */
#define highlighting_lexer_HASKELL			SCLEX_HASKELL
static const HLStyle highlighting_styles_HASKELL[] =
{
	{ SCE_HA_DEFAULT,		"default"		 },
	{ SCE_HA_COMMENTLINE,	"commentline"	 },
	{ SCE_HA_COMMENTBLOCK,	"commentblock"	 },
	{ SCE_HA_COMMENTBLOCK2,	"commentblock2"	 },
	{ SCE_HA_COMMENTBLOCK3,	"commentblock3"	 },
	{ SCE_HA_NUMBER,		"number"		 },
	{ SCE_HA_KEYWORD,		"keyword"		 },
	{ SCE_HA_IMPORT,		"import"		 },
	{ SCE_HA_STRING,		"string"		 },
	{ SCE_HA_CHARACTER,		"character"		 },
	{ SCE_HA_CLASS,			"class"			 },
	{ SCE_HA_OPERATOR,		"operator"		 },
	{ SCE_HA_IDENTIFIER,	"identifier"	 },
	{ SCE_HA_INSTANCE,		"instance"		 },
	{ SCE_HA_CAPITAL,		"capital"		 },
	{ SCE_HA_MODULE,		"module"		 },
	{ SCE_HA_DATA,			"data"			 }
};
static const HLKeyword highlighting_keywords_HASKELL[] =
{
	{ 0, "keywords" }
};
#define highlighting_properties_HASKELL		EMPTY_PROPERTIES


/* HAXE */
#define highlighting_lexer_HAXE			SCLEX_CPP
#define highlighting_styles_HAXE		highlighting_styles_C
static const HLKeyword highlighting_keywords_HAXE[] =
{
	{ 0, "primary" },
	{ 1, "secondary" },
	{ 3, "classes" }
};
#define highlighting_properties_HAXE	highlighting_properties_C


/* Java */
#define highlighting_lexer_JAVA			SCLEX_CPP
#define highlighting_styles_JAVA		highlighting_styles_C
static const HLKeyword highlighting_keywords_JAVA[] =
{
	{ 0, "primary" },
	/* SCI_SETKEYWORDS = 1 - secondary + global tags file types, see below */
	{ 1, "secondary", TRUE },
	{ 2, "doccomment" },
	/* SCI_SETKEYWORDS = 3 is for current session types - see editor_lexer_get_type_keyword_idx() */
	{ 4, "typedefs" }
};
#define highlighting_properties_JAVA	highlighting_properties_C


/* JavaScript */
#define highlighting_lexer_JS		SCLEX_CPP
#define highlighting_styles_JS		highlighting_styles_C
static const HLKeyword highlighting_keywords_JS[] =
{
	{ 0, "primary" },
	{ 1, "secondary" }
};
#define highlighting_properties_JS	highlighting_properties_C


/* LaTeX */
#define highlighting_lexer_LATEX		SCLEX_LATEX
static const HLStyle highlighting_styles_LATEX[] =
{
	{ SCE_L_DEFAULT,	"default"	 },
	{ SCE_L_COMMAND,	"command"	 },
	{ SCE_L_TAG,		"tag"		 },
	{ SCE_L_MATH,		"math"		 },
	{ SCE_L_COMMENT,	"comment"	 },
	{ SCE_L_TAG2,		"tag2"		 },
	{ SCE_L_MATH2,		"math2"		 },
	{ SCE_L_COMMENT2,	"comment2"	 },
	{ SCE_L_VERBATIM,	"verbatim"	 },
	{ SCE_L_SHORTCMD,	"shortcmd"	 },
	{ SCE_L_SPECIAL,	"special"	 },
	{ SCE_L_CMDOPT,		"cmdopt"	 },
	{ SCE_L_ERROR,		"error"		 }
};
static const HLKeyword highlighting_keywords_LATEX[] =
{
	{ 0, "primary" }
};
#define highlighting_properties_LATEX	EMPTY_PROPERTIES


/* Lisp */
#define highlighting_lexer_LISP			SCLEX_LISP
static const HLStyle highlighting_styles_LISP[] =
{
	{ SCE_LISP_DEFAULT,			"default"		 },
	{ SCE_LISP_COMMENT,			"comment"		 },
	{ SCE_LISP_MULTI_COMMENT,	"multicomment"	 },
	{ SCE_LISP_NUMBER,			"number"		 },
	{ SCE_LISP_KEYWORD,			"keyword"		 },
	{ SCE_LISP_SYMBOL,			"symbol"		 },
	{ SCE_LISP_STRING,			"string"		 },
	{ SCE_LISP_STRINGEOL,		"stringeol"		 },
	{ SCE_LISP_IDENTIFIER,		"identifier"	 }, 
	{ SCE_LISP_OPERATOR,		"operator"		 },
	{ SCE_LISP_SPECIAL,			"special"		 },
	{ SCE_LISP_KEYWORD_KW,		"keywordkw"		 }
};
static const HLKeyword highlighting_keywords_LISP[] =
{
	{ 0, "keywords" },
	{ 1, "special_keywords" }
};
#define highlighting_properties_LISP	EMPTY_PROPERTIES


/* Lua */
#define highlighting_lexer_LUA			SCLEX_LUA
static const HLStyle highlighting_styles_LUA[] =
{
	{ SCE_LUA_DEFAULT,			"default"		 },
	{ SCE_LUA_COMMENT,			"comment"		 },
	{ SCE_LUA_COMMENTLINE,		"commentline"	 },
	{ SCE_LUA_COMMENTDOC,		"commentdoc"	 },
	{ SCE_LUA_NUMBER,			"number"		 },
	{ SCE_LUA_WORD,				"word"			 },
	{ SCE_LUA_STRING,			"string"		 },
	{ SCE_LUA_CHARACTER,		"character"		 },
	{ SCE_LUA_LITERALSTRING,	"literalstring"	 },
	{ SCE_LUA_PREPROCESSOR,		"preprocessor"	 },
	{ SCE_LUA_OPERATOR,			"operator"		 },
	{ SCE_LUA_IDENTIFIER,		"identifier"	 },
	{ SCE_LUA_STRINGEOL,		"stringeol"		 },
	{ SCE_LUA_WORD2,			"function_basic" },
	{ SCE_LUA_WORD3,			"function_other" },
	{ SCE_LUA_WORD4,			"coroutines"	 },
	{ SCE_LUA_WORD5,			"word5"			 },
	{ SCE_LUA_WORD6,			"word6"			 },
	{ SCE_LUA_WORD7,			"word7"			 },
	{ SCE_LUA_WORD8,			"word8"			 },
	{ SCE_LUA_LABEL,			"label"			 }
};
static const HLKeyword highlighting_keywords_LUA[] =
{
	{ 0, "keywords" },
	{ 1, "function_basic" },
	{ 2, "function_other" },
	{ 3, "coroutines" },
	{ 4, "user1" },
	{ 5, "user2" },
	{ 6, "user3" },
	{ 7, "user4" }
};
#define highlighting_properties_LUA		EMPTY_PROPERTIES


/* Makefile */
#define highlighting_lexer_MAKE			SCLEX_MAKEFILE
static const HLStyle highlighting_styles_MAKE[] =
{
	{ SCE_MAKE_DEFAULT,			"default"		 },
	{ SCE_MAKE_COMMENT,			"comment"		 },
	{ SCE_MAKE_PREPROCESSOR,	"preprocessor"	 },
	{ SCE_MAKE_IDENTIFIER,		"identifier"	 },
	{ SCE_MAKE_OPERATOR,		"operator"		 },
	{ SCE_MAKE_TARGET,			"target"		 },
	{ SCE_MAKE_IDEOL,			"ideol"			 }
};
#define highlighting_keywords_MAKE		EMPTY_KEYWORDS
#define highlighting_properties_MAKE	EMPTY_PROPERTIES


/* Markdown */
#define highlighting_lexer_MARKDOWN			SCLEX_MARKDOWN
static const HLStyle highlighting_styles_MARKDOWN[] =
{
	{ SCE_MARKDOWN_DEFAULT,		"default"	 },
	{ SCE_MARKDOWN_LINE_BEGIN,	"default"	 }, /* FIXME: avoid in-code re-mappings */
	{ SCE_MARKDOWN_PRECHAR,		"default"	 },
	{ SCE_MARKDOWN_STRONG1,		"strong"	 },
	{ SCE_MARKDOWN_STRONG2,		"strong"	 },
	{ SCE_MARKDOWN_EM1,			"emphasis"	 },
	{ SCE_MARKDOWN_EM2,			"emphasis"	 },
	{ SCE_MARKDOWN_HEADER1,		"header1"	 },
	{ SCE_MARKDOWN_HEADER2,		"header2"	 },
	{ SCE_MARKDOWN_HEADER3,		"header3"	 },
	{ SCE_MARKDOWN_HEADER4,		"header4"	 },
	{ SCE_MARKDOWN_HEADER5,		"header5"	 },
	{ SCE_MARKDOWN_HEADER6,		"header6"	 },
	{ SCE_MARKDOWN_ULIST_ITEM,	"ulist_item" },
	{ SCE_MARKDOWN_OLIST_ITEM,	"olist_item" },
	{ SCE_MARKDOWN_BLOCKQUOTE,	"blockquote" },
	{ SCE_MARKDOWN_STRIKEOUT,	"strikeout"	 },
	{ SCE_MARKDOWN_HRULE,		"hrule"		 },
	{ SCE_MARKDOWN_LINK,		"link"		 },
	{ SCE_MARKDOWN_CODE,		"code"		 },
	{ SCE_MARKDOWN_CODE2,		"code"		 },
	{ SCE_MARKDOWN_CODEBK,		"codebk"	 }
};
#define highlighting_keywords_MARKDOWN		EMPTY_KEYWORDS
#define highlighting_properties_MARKDOWN	EMPTY_PROPERTIES


/* Matlab */
#define highlighting_lexer_MATLAB		SCLEX_OCTAVE /* not MATLAB to support Octave's # comments */
static const HLStyle highlighting_styles_MATLAB[] =
{
	{ SCE_MATLAB_DEFAULT,			"default"			 },
	{ SCE_MATLAB_COMMENT,			"comment"			 },
	{ SCE_MATLAB_COMMAND,			"command"			 },
	{ SCE_MATLAB_NUMBER,			"number"			 },
	{ SCE_MATLAB_KEYWORD,			"keyword"			 },
	{ SCE_MATLAB_STRING,			"string"			 },
	{ SCE_MATLAB_OPERATOR,			"operator"			 },
	{ SCE_MATLAB_IDENTIFIER,		"identifier"		 },
	{ SCE_MATLAB_DOUBLEQUOTESTRING,	"doublequotedstring" }
};
static const HLKeyword highlighting_keywords_MATLAB[] =
{
	{ 0, "primary" }
};
#define highlighting_properties_MATLAB	EMPTY_PROPERTIES


/* NSIS */
#define highlighting_lexer_NSIS			SCLEX_NSIS
static const HLStyle highlighting_styles_NSIS[] =
{
	{ SCE_NSIS_DEFAULT,			"default"		 },
	{ SCE_NSIS_COMMENT,			"comment"		 },
	{ SCE_NSIS_STRINGDQ,		"stringdq"		 },
	{ SCE_NSIS_STRINGLQ,		"stringlq"		 },
	{ SCE_NSIS_STRINGRQ,		"stringrq"		 },
	{ SCE_NSIS_FUNCTION,		"function"		 },
	{ SCE_NSIS_VARIABLE,		"variable"		 },
	{ SCE_NSIS_LABEL,			"label"			 },
	{ SCE_NSIS_USERDEFINED,		"userdefined"	 },
	{ SCE_NSIS_SECTIONDEF,		"sectiondef"	 },
	{ SCE_NSIS_SUBSECTIONDEF,	"subsectiondef"	 },
	{ SCE_NSIS_IFDEFINEDEF,		"ifdefinedef"	 },
	{ SCE_NSIS_MACRODEF,		"macrodef"		 },
	{ SCE_NSIS_STRINGVAR,		"stringvar"		 },
	{ SCE_NSIS_NUMBER,			"number"		 },
	{ SCE_NSIS_SECTIONGROUP,	"sectiongroup"	 },
	{ SCE_NSIS_PAGEEX,			"pageex"		 },
	{ SCE_NSIS_FUNCTIONDEF,		"functiondef"	 },
	{ SCE_NSIS_COMMENTBOX,		"commentbox"	 }
};
static const HLKeyword highlighting_keywords_NSIS[] =
{
	{ 0, "functions" },
	{ 1, "variables" },
	{ 2, "lables" },
	{ 3, "userdefined" }
};
#define highlighting_properties_NSIS	EMPTY_PROPERTIES


/* Pascal */
#define highlighting_lexer_PASCAL		SCLEX_PASCAL
static const HLStyle highlighting_styles_PASCAL[] =
{
	{ SCE_PAS_DEFAULT,			"default"		 },
	{ SCE_PAS_IDENTIFIER,		"identifier"	 },
	{ SCE_PAS_COMMENT,			"comment"		 },
	{ SCE_PAS_COMMENT2,			"comment2"		 },
	{ SCE_PAS_COMMENTLINE,		"commentline"	 },
	{ SCE_PAS_PREPROCESSOR,		"preprocessor"	 },
	{ SCE_PAS_PREPROCESSOR2,	"preprocessor2"	 },
	{ SCE_PAS_NUMBER,			"number"		 },
	{ SCE_PAS_HEXNUMBER,		"hexnumber"		 },
	{ SCE_PAS_WORD,				"word"			 },
	{ SCE_PAS_STRING,			"string"		 },
	{ SCE_PAS_STRINGEOL,		"stringeol"		 },
	{ SCE_PAS_CHARACTER,		"character"		 },
	{ SCE_PAS_OPERATOR,			"operator"		 },
	{ SCE_PAS_ASM,				"asm"			 }
};
static const HLKeyword highlighting_keywords_PASCAL[] =
{
	{ 0, "primary" }
};
#define highlighting_properties_PASCAL	EMPTY_PROPERTIES


/* Perl */
#define highlighting_lexer_PERL			SCLEX_PERL
static const HLStyle highlighting_styles_PERL[] =
{
	{ SCE_PL_DEFAULT,			"default"			 },
	{ SCE_PL_ERROR,				"error"				 },
	{ SCE_PL_COMMENTLINE,		"commentline"		 },
	{ SCE_PL_NUMBER,			"number"			 },
	{ SCE_PL_WORD,				"word"				 },
	{ SCE_PL_STRING,			"string"			 },
	{ SCE_PL_CHARACTER,			"character"			 },
	{ SCE_PL_PREPROCESSOR,		"preprocessor"		 },
	{ SCE_PL_OPERATOR,			"operator"			 },
	{ SCE_PL_IDENTIFIER,		"identifier"		 },
	{ SCE_PL_SCALAR,			"scalar"			 },
	{ SCE_PL_POD,				"pod"				 },
	{ SCE_PL_REGEX,				"regex"				 },
	{ SCE_PL_ARRAY,				"array"				 },
	{ SCE_PL_HASH,				"hash"				 },
	{ SCE_PL_SYMBOLTABLE,		"symboltable"		 },
	{ SCE_PL_BACKTICKS,			"backticks"			 },
	{ SCE_PL_POD_VERB,			"pod_verbatim"		 },
	{ SCE_PL_REGSUBST,			"reg_subst"			 },
	{ SCE_PL_DATASECTION,		"datasection"		 },
	{ SCE_PL_HERE_DELIM,		"here_delim"		 },
	{ SCE_PL_HERE_Q,			"here_q"			 },
	{ SCE_PL_HERE_QQ,			"here_qq"			 },
	{ SCE_PL_HERE_QX,			"here_qx"			 },
	{ SCE_PL_STRING_Q,			"string_q"			 },
	{ SCE_PL_STRING_QQ,			"string_qq"			 },
	{ SCE_PL_STRING_QX,			"string_qx"			 },
	{ SCE_PL_STRING_QR,			"string_qr"			 },
	{ SCE_PL_STRING_QW,			"string_qw"			 },
	{ SCE_PL_VARIABLE_INDEXER,	"variable_indexer"	 },
	{ SCE_PL_PUNCTUATION,		"punctuation"		 },
	{ SCE_PL_LONGQUOTE,			"longquote"			 },
	{ SCE_PL_SUB_PROTOTYPE,		"sub_prototype"		 },
	{ SCE_PL_FORMAT_IDENT,		"format_ident"		 },
	{ SCE_PL_FORMAT,			"format"			 },
	{ SCE_PL_STRING_VAR,		"string_var"		 },
	{ SCE_PL_XLAT,				"xlat"				 },
	{ SCE_PL_REGEX_VAR,			"regex_var"			 },
	{ SCE_PL_REGSUBST_VAR,		"regsubst_var"		 },
	{ SCE_PL_BACKTICKS_VAR,		"backticks_var"		 },
	{ SCE_PL_HERE_QQ_VAR,		"here_qq_var"		 },
	{ SCE_PL_HERE_QX_VAR,		"here_qx_var"		 },
	{ SCE_PL_STRING_QQ_VAR,		"string_qq_var"		 },
	{ SCE_PL_STRING_QX_VAR,		"string_qx_var"		 },
	{ SCE_PL_STRING_QR_VAR,		"string_qr_var"		 }
};
static const HLKeyword highlighting_keywords_PERL[] =
{
	{ 0, "primary" }
};
#define highlighting_properties_PERL	EMPTY_PROPERTIES


/* PO (gettext) */
#define highlighting_lexer_PO		SCLEX_PO
static const HLStyle highlighting_styles_PO[] =
{
	{ SCE_PO_DEFAULT,		"default"		 },
	{ SCE_PO_COMMENT,		"comment"		 },
	{ SCE_PO_MSGID,			"msgid"			 },
	{ SCE_PO_MSGID_TEXT,	"msgid_text"	 },
	{ SCE_PO_MSGSTR,		"msgstr"		 },
	{ SCE_PO_MSGSTR_TEXT,	"msgstr_text"	 },
	{ SCE_PO_MSGCTXT,		"msgctxt"		 },
	{ SCE_PO_MSGCTXT_TEXT,	"msgctxt_text"	 },
	{ SCE_PO_FUZZY,			"fuzzy"			 }
};
#define highlighting_keywords_PO	EMPTY_KEYWORDS
#define highlighting_properties_PO	EMPTY_PROPERTIES


/* Python */
#define highlighting_lexer_PYTHON		SCLEX_PYTHON
static const HLStyle highlighting_styles_PYTHON[] =
{
	{ SCE_P_DEFAULT,		"default"		 },
	{ SCE_P_COMMENTLINE,	"commentline"	 },
	{ SCE_P_NUMBER,			"number"		 },
	{ SCE_P_STRING,			"string"		 },
	{ SCE_P_CHARACTER,		"character"		 },
	{ SCE_P_WORD,			"word"			 },
	{ SCE_P_TRIPLE,			"triple"		 },
	{ SCE_P_TRIPLEDOUBLE,	"tripledouble"	 },
	{ SCE_P_CLASSNAME,		"classname"		 },
	{ SCE_P_DEFNAME,		"defname"		 },
	{ SCE_P_OPERATOR,		"operator"		 },
	{ SCE_P_IDENTIFIER,		"identifier"	 },
	{ SCE_P_COMMENTBLOCK,	"commentblock"	 },
	{ SCE_P_STRINGEOL,		"stringeol"		 },
	{ SCE_P_WORD2,			"word2"			 },
	{ SCE_P_DECORATOR,		"decorator"		 }
};
static const HLKeyword highlighting_keywords_PYTHON[] =
{
	{ 0, "primary" },
	{ 1, "identifiers" }
};
#define highlighting_properties_PYTHON	EMPTY_PROPERTIES


/* R */
#define highlighting_lexer_R		SCLEX_R
static const HLStyle highlighting_styles_R[] =
{
	{ SCE_R_DEFAULT,	"default"	 },
	{ SCE_R_COMMENT,	"comment"	 },
	{ SCE_R_KWORD,		"kword"		 },
	{ SCE_R_OPERATOR,	"operator"	 },
	{ SCE_R_BASEKWORD,	"basekword"	 },
	{ SCE_R_OTHERKWORD,	"otherkword" },
	{ SCE_R_NUMBER,		"number"	 },
	{ SCE_R_STRING,		"string"	 },
	{ SCE_R_STRING2,	"string2"	 },
	{ SCE_R_IDENTIFIER,	"identifier" },
	{ SCE_R_INFIX,		"infix"		 },
	{ SCE_R_INFIXEOL,	"infixeol"	 }
};
static const HLKeyword highlighting_keywords_R[] =
{
	{ 0, "primary" },
	{ 1, "package" },
	{ 2, "package_other" }
};
#define highlighting_properties_R	EMPTY_PROPERTIES


/* Ruby */
#define highlighting_lexer_RUBY			SCLEX_RUBY
static const HLStyle highlighting_styles_RUBY[] =
{
	{ SCE_RB_DEFAULT,		"default"		 },
	{ SCE_RB_COMMENTLINE,	"commentline"	 },
	{ SCE_RB_NUMBER,		"number"		 },
	{ SCE_RB_STRING,		"string"		 },
	{ SCE_RB_CHARACTER,		"character"		 },
	{ SCE_RB_WORD,			"word"			 },
	{ SCE_RB_GLOBAL,		"global"		 },
	{ SCE_RB_SYMBOL,		"symbol"		 },
	{ SCE_RB_CLASSNAME,		"classname"		 },
	{ SCE_RB_DEFNAME,		"defname"		 },
	{ SCE_RB_OPERATOR,		"operator"		 },
	{ SCE_RB_IDENTIFIER,	"identifier"	 },
	{ SCE_RB_MODULE_NAME,	"modulename"	 },
	{ SCE_RB_BACKTICKS,		"backticks"		 },
	{ SCE_RB_INSTANCE_VAR,	"instancevar"	 },
	{ SCE_RB_CLASS_VAR,		"classvar"		 },
	{ SCE_RB_DATASECTION,	"datasection"	 },
	{ SCE_RB_HERE_DELIM,	"heredelim"		 },
	{ SCE_RB_WORD_DEMOTED,	"worddemoted"	 },
	{ SCE_RB_STDIN,			"stdin"			 },
	{ SCE_RB_STDOUT,		"stdout"		 },
	{ SCE_RB_STDERR,		"stderr"		 },
	{ SCE_RB_REGEX,			"regex"			 },
	{ SCE_RB_HERE_Q,		"here_q"		 },
	{ SCE_RB_HERE_QQ,		"here_qq"		 },
	{ SCE_RB_HERE_QX,		"here_qx"		 },
	{ SCE_RB_STRING_Q,		"string_q"		 },
	{ SCE_RB_STRING_QQ,		"string_qq"		 },
	{ SCE_RB_STRING_QX,		"string_qx"		 },
	{ SCE_RB_STRING_QR,		"string_qr"		 },
	{ SCE_RB_STRING_QW,		"string_qw"		 },
	{ SCE_RB_UPPER_BOUND,	"upper_bound"	 },
	{ SCE_RB_ERROR,			"error"			 },
	{ SCE_RB_POD,			"pod"			 }
};
static const HLKeyword highlighting_keywords_RUBY[] =
{
	{ 0, "primary" }
};
#define highlighting_properties_RUBY	EMPTY_PROPERTIES


/* SH */
#define highlighting_lexer_SH		SCLEX_BASH
static const HLStyle highlighting_styles_SH[] =
{
	{ SCE_SH_DEFAULT,		"default"		 },
	{ SCE_SH_COMMENTLINE,	"commentline"	 },
	{ SCE_SH_NUMBER,		"number"		 },
	{ SCE_SH_WORD,			"word"			 },
	{ SCE_SH_STRING,		"string"		 },
	{ SCE_SH_CHARACTER,		"character"		 },
	{ SCE_SH_OPERATOR,		"operator"		 },
	{ SCE_SH_IDENTIFIER,	"identifier"	 },
	{ SCE_SH_BACKTICKS,		"backticks"		 },
	{ SCE_SH_PARAM,			"param"			 },
	{ SCE_SH_SCALAR,		"scalar"		 },
	{ SCE_SH_ERROR,			"error"			 },
	{ SCE_SH_HERE_DELIM,	"here_delim"	 },
	{ SCE_SH_HERE_Q,		"here_q"		 }
};
static const HLKeyword highlighting_keywords_SH[] =
{
	{ 0, "primary" }
};
#define highlighting_properties_SH	EMPTY_PROPERTIES


/* SQL */
#define highlighting_lexer_SQL			SCLEX_SQL
static const HLStyle highlighting_styles_SQL[] =
{
	{ SCE_SQL_DEFAULT,					"default"				 },
	{ SCE_SQL_COMMENT,					"comment"				 },
	{ SCE_SQL_COMMENTLINE,				"commentline"			 },
	{ SCE_SQL_COMMENTDOC,				"commentdoc"			 },
	{ SCE_SQL_COMMENTLINEDOC,			"commentlinedoc"		 },
	{ SCE_SQL_COMMENTDOCKEYWORD,		"commentdockeyword"		 },
	{ SCE_SQL_COMMENTDOCKEYWORDERROR,	"commentdockeyworderror" },
	{ SCE_SQL_NUMBER,					"number"				 },
	{ SCE_SQL_WORD,						"word"					 },
	{ SCE_SQL_WORD2,					"word2"					 },
	{ SCE_SQL_STRING,					"string"				 },
	{ SCE_SQL_CHARACTER,				"character"				 },
	{ SCE_SQL_OPERATOR,					"operator"				 },
	{ SCE_SQL_IDENTIFIER,				"identifier"			 },
	{ SCE_SQL_SQLPLUS,					"sqlplus"				 },
	{ SCE_SQL_SQLPLUS_PROMPT,			"sqlplus_prompt"		 },
	{ SCE_SQL_SQLPLUS_COMMENT,			"sqlplus_comment"		 },
	{ SCE_SQL_QUOTEDIDENTIFIER,			"quotedidentifier"		 }
	/* these are for user-defined keywords we don't set yet */
	/*{ SCE_SQL_USER1,					"user1"					 },
	{ SCE_SQL_USER2,					"user2"					 },
	{ SCE_SQL_USER3,					"user3"					 },
	{ SCE_SQL_USER4,					"user4"					 }*/
};
static const HLKeyword highlighting_keywords_SQL[] =
{
	{ 0, "keywords" }
};
#define highlighting_properties_SQL		EMPTY_PROPERTIES


/* TCL */
#define highlighting_lexer_TCL			SCLEX_TCL
static const HLStyle highlighting_styles_TCL[] =
{
	{ SCE_TCL_DEFAULT,			"default"		 },
	{ SCE_TCL_COMMENT,			"comment"		 },
	{ SCE_TCL_COMMENTLINE,		"commentline"	 },
	{ SCE_TCL_NUMBER,			"number"		 },
	{ SCE_TCL_OPERATOR,			"operator"		 },
	{ SCE_TCL_IDENTIFIER,		"identifier"	 },
	{ SCE_TCL_WORD_IN_QUOTE,	"wordinquote"	 },
	{ SCE_TCL_IN_QUOTE,			"inquote"		 },
	{ SCE_TCL_SUBSTITUTION,		"substitution"	 },
	{ SCE_TCL_MODIFIER,			"modifier"		 },
	{ SCE_TCL_EXPAND,			"expand"		 },
	{ SCE_TCL_WORD,				"wordtcl"		 },
	{ SCE_TCL_WORD2,			"wordtk"		 },
	{ SCE_TCL_WORD3,			"worditcl"		 },
	{ SCE_TCL_WORD4,			"wordtkcmds"	 },
	{ SCE_TCL_WORD5,			"wordexpand"	 },
	{ SCE_TCL_COMMENT_BOX,		"commentbox"	 },
	{ SCE_TCL_BLOCK_COMMENT,	"blockcomment"	 }
	/* these are for user-defined keywords we don't set yet */
	/*{ SCE_TCL_WORD6,			"user2"			 },
	{ SCE_TCL_WORD7,			"user3"			 },
	{ SCE_TCL_WORD8,			"user4"			 }*/
};
static const HLKeyword highlighting_keywords_TCL[] =
{
	{ 0, "tcl" },
	{ 1, "tk" },
	{ 2, "itcl" },
	{ 3, "tkcommands" },
	{ 4, "expand" }
};
#define highlighting_properties_TCL		EMPTY_PROPERTIES


/* Txt2Tags */
#define highlighting_lexer_TXT2TAGS			SCLEX_TXT2TAGS
static const HLStyle highlighting_styles_TXT2TAGS[] =
{
	{ SCE_TXT2TAGS_DEFAULT,		"default"	 },
	{ SCE_TXT2TAGS_LINE_BEGIN,	"default"	 }, /* XIFME: remappings should be avoided */
	{ SCE_TXT2TAGS_PRECHAR,		"default"	 },
	{ SCE_TXT2TAGS_STRONG1,		"strong"	 },
	{ SCE_TXT2TAGS_STRONG2,		"strong"	 },
	{ SCE_TXT2TAGS_EM1,			"emphasis"	 },
	{ SCE_TXT2TAGS_EM2,			"underlined" }, /* WTF? */
	{ SCE_TXT2TAGS_HEADER1,		"header1"	 },
	{ SCE_TXT2TAGS_HEADER2,		"header2"	 },
	{ SCE_TXT2TAGS_HEADER3,		"header3"	 },
	{ SCE_TXT2TAGS_HEADER4,		"header4"	 },
	{ SCE_TXT2TAGS_HEADER5,		"header5"	 },
	{ SCE_TXT2TAGS_HEADER6,		"header6"	 },
	{ SCE_TXT2TAGS_ULIST_ITEM,	"ulist_item" },
	{ SCE_TXT2TAGS_OLIST_ITEM,	"olist_item" },
	{ SCE_TXT2TAGS_BLOCKQUOTE,	"blockquote" },
	{ SCE_TXT2TAGS_STRIKEOUT,	"strikeout"	 },
	{ SCE_TXT2TAGS_HRULE,		"hrule"		 },
	{ SCE_TXT2TAGS_LINK,		"link"		 },
	{ SCE_TXT2TAGS_CODE,		"code"		 },
	{ SCE_TXT2TAGS_CODE2,		"code"		 },
	{ SCE_TXT2TAGS_CODEBK,		"codebk"	 },
	{ SCE_TXT2TAGS_COMMENT,		"comment"	 },
	{ SCE_TXT2TAGS_OPTION,		"option"	 },
	{ SCE_TXT2TAGS_PREPROC,		"preproc"	 },
	{ SCE_TXT2TAGS_POSTPROC,	"postproc"	 }
};
#define highlighting_keywords_TXT2TAGS		EMPTY_KEYWORDS
#define highlighting_properties_TXT2TAGS	EMPTY_PROPERTIES


/* VHDL */
#define highlighting_lexer_VHDL			SCLEX_VHDL
static const HLStyle highlighting_styles_VHDL[] =
{
	{ SCE_VHDL_DEFAULT,			"default"			 },
	{ SCE_VHDL_COMMENT,			"comment"			 },
	{ SCE_VHDL_COMMENTLINEBANG,	"comment_line_bang"	 },
	{ SCE_VHDL_NUMBER,			"number"			 },
	{ SCE_VHDL_STRING,			"string"			 },
	{ SCE_VHDL_OPERATOR,		"operator"			 },
	{ SCE_VHDL_IDENTIFIER,		"identifier"		 },
	{ SCE_VHDL_STRINGEOL,		"stringeol"			 },
	{ SCE_VHDL_KEYWORD,			"keyword"			 },
	{ SCE_VHDL_STDOPERATOR,		"stdoperator"		 },
	{ SCE_VHDL_ATTRIBUTE,		"attribute"			 },
	{ SCE_VHDL_STDFUNCTION,		"stdfunction"		 },
	{ SCE_VHDL_STDPACKAGE,		"stdpackage"		 },
	{ SCE_VHDL_STDTYPE,			"stdtype"			 },
	{ SCE_VHDL_USERWORD,		"userword"			 }
};
static const HLKeyword highlighting_keywords_VHDL[] =
{
	{ 0, "keywords" },
	{ 1, "operators" },
	{ 2, "attributes" },
	{ 3, "std_functions" },
	{ 4, "std_packages" },
	{ 5, "std_types" },
	{ 6, "userwords" },
};
#define highlighting_properties_VHDL	EMPTY_PROPERTIES


/* Verilog */
#define highlighting_lexer_VERILOG			SCLEX_VERILOG
static const HLStyle highlighting_styles_VERILOG[] =
{
	{ SCE_V_DEFAULT,			"default"			 },
	{ SCE_V_COMMENT,			"comment"			 },
	{ SCE_V_COMMENTLINE,		"comment_line"		 },
	{ SCE_V_COMMENTLINEBANG,	"comment_line_bang"	 },
	{ SCE_V_NUMBER,				"number"			 },
	{ SCE_V_WORD,				"word"				 },
	{ SCE_V_STRING,				"string"			 },
	{ SCE_V_WORD2,				"word2"				 },
	{ SCE_V_WORD3,				"word3"				 },
	{ SCE_V_PREPROCESSOR,		"preprocessor"		 },
	{ SCE_V_OPERATOR,			"operator"			 },
	{ SCE_V_IDENTIFIER,			"identifier"		 },
	{ SCE_V_STRINGEOL,			"stringeol"			 },
	{ SCE_V_USER,				"userword"			 }
};
static const HLKeyword highlighting_keywords_VERILOG[] =
{
	{ 0, "word" },
	{ 1, "word2" },
	{ 2, "word3" }
};
#define highlighting_properties_VERILOG		EMPTY_PROPERTIES


/* YAML */
#define highlighting_lexer_YAML			SCLEX_YAML
static const HLStyle highlighting_styles_YAML[] =
{
	{ SCE_YAML_DEFAULT,		"default"	 },
	{ SCE_YAML_COMMENT,		"comment"	 },
	{ SCE_YAML_IDENTIFIER,	"identifier" },
	{ SCE_YAML_KEYWORD,		"keyword"	 },
	{ SCE_YAML_NUMBER,		"number"	 },
	{ SCE_YAML_REFERENCE,	"reference"	 },
	{ SCE_YAML_DOCUMENT,	"document"	 },
	{ SCE_YAML_TEXT,		"text"		 },
	{ SCE_YAML_ERROR,		"error"		 },
	{ SCE_YAML_OPERATOR,	"operator"	 }
};
static const HLKeyword highlighting_keywords_YAML[] =
{
	{ 0, "keywords" }
};
#define highlighting_properties_YAML	EMPTY_PROPERTIES


#endif

