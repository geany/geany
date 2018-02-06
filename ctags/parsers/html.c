/*
*   Copyright (c) 2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for generating tags for HTML language
*   files.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */
#include "parse.h"
#include "routines.h"

static tagRegexTable htmlTagRegexTable [] = {
#define POSSIBLE_ATTRIBUTES "([ \t]+[a-z]+=\"?[^>\"]*\"?)*"
	{"<a"
	 POSSIBLE_ATTRIBUTES "[ \t]+name=\"?([^>\"]+)\"?" POSSIBLE_ATTRIBUTES
	 "[ \t]*>", "\\2",
	 "a,anchor,named anchors", "i", NULL},
	{"^[ \t]*function[ \t]*([A-Za-z0-9_]+)[ \t]*\\(", "\\1",
	 "f,function,JavaScript functions", NULL, NULL},

/* the following matches headings with tags inside like
 * <h1><a href="#id109"><i>Some Text</i></a></h1>
 * and
 * <h1>Some Text</h1> */
#define SPACES "[ \t]*"
#define ATTRS "[^>]*"
#define INNER_HEADING \
	ATTRS ">" SPACES "(<" ATTRS ">" SPACES ")*([^<]+).*"

	{"<h1" INNER_HEADING "</h1>", "\\2",
	 "n,namespace,H1 heading", "i", NULL},

	{"<h2" INNER_HEADING "</h2>", "\\2",
	 "c,class,H2 heading", "i", NULL},

	{"<h3" INNER_HEADING "</h3>", "\\2",
	 "v,variable,H3 heading", "i", NULL},
};

/*
*   FUNCTION DEFINITIONS
*/

/* Create parser definition structure */
extern parserDefinition* HtmlParser (void)
{
	static const char *const extensions [] = { "htm", "html", NULL };
	parserDefinition *const def = parserNew ("HTML");
	def->extensions = extensions;
	def->tagRegexTable = htmlTagRegexTable;
	def->tagRegexCount = ARRAY_SIZE (htmlTagRegexTable);
	def->method     = METHOD_NOT_CRAFTED|METHOD_REGEX;
	return def;
}
