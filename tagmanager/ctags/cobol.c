/*
*   Copyright (c) 2000-2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for COBOL language
*   files.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */
#include "parse.h"

/*
*   FUNCTION DEFINITIONS
*/

static void installCobolRegex (const langType language)
{
   addTagRegex (language, "^[ \t]+[0-9]+[ \t]+(?!PIC|FILLER|SIGNED-INT|SIGNED-LONG|SIGNED-SHORT|UNSIGNED-INT|UNSIGNED-LONG|UNSIGNED-SHORT)([A-Z0-9][A-Z0-9-]+)[ \t]+(BLANK|OCCURS|IS|JUST|PIC|REDEFINES|RENAMES|SIGN|SYNC|USAGE|VALUE|SIGNED-INT|SIGNED-LONG|SIGNED-SHORT|UNSIGNED-INT|UNSIGNED-LONG|UNSIGNED-SHORT)",
		"\\1", "d,variable,data items", "i");
	addTagRegex (language, "^[ \t]{1,4}[FSR]D[ \t]+([A-Z0-9][A-Z0-9-]*)\\.{0,1}",
		"\\1", "f,function,file descriptions (FD, SD, RD)", "i");
	addTagRegex (language, "^[ \t]{1,4}[0-9]+[ \t]+(?!PIC|FILLER|SIGNED-INT|SIGNED-LONG|SIGNED-SHORT|UNSIGNED-INT|UNSIGNED-LONG|UNSIGNED-SHORT)([A-Z0-9][A-Z0-9-]*)\\.",
		"\\1", "g,struct,group items", "i");
	addTagRegex (language, "^[ \t]{1,4}(?!AUTHOR|PROGRAM-ID|FILE-CONTROL|I-O-CONTROL|OBJECT-COMPUTER|SOURCE-COMPUTER|DATE-COMPILED|SPECIAL-NAMES)([A-Z0-9][A-Z0-9-]*)\\.",
		"\\1", "p,macro,paragraphs", "i");
	addTagRegex (language, "^[ \t]{1,4}PROGRAM-ID\\.[ \t]+([A-Z0-9][A-Z0-9-]*)\\.",
		"\\1", "P,class,program ids", "i");
	addTagRegex (language, "^[ \t]{1,4}([A-Z0-9][A-Z0-9-]*)[ \t]+SECTION\\.",
		"\\1", "n,namespace,sections", "i");
}

extern parserDefinition* CobolParser (void)
{
	static const char *const extensions [] = {
			"cbl", "cob", "cpy", "CBL", "COB", NULL };
	parserDefinition* def = parserNew ("Cobol");
	def->extensions = extensions;
	def->initialize = installCobolRegex;
	def->regex      = TRUE;
	return def;
}

/* vi:set tabstop=4 shiftwidth=4: */
