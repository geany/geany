/*
*   Copyright (c) 2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for HTML language
*   files.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */
#include "parse.h"

/*
*   FUNCTION DEFINITIONS
*/

static void installHtmlRegex (const langType language)
{
#define POSSIBLE_ATTRIBUTES "([ \t]+[a-z]+=\"?[^>\"]*\"?)*"

	addTagRegex (language,
		"<a"
		POSSIBLE_ATTRIBUTES
		"[ \t]+name=\"?([^>\"]+)\"?"
		POSSIBLE_ATTRIBUTES
		"[ \t]*>",
		"\\2", "m,member,named anchors", "i");

	addTagRegex (language, "^[ \t]*function[ \t]*([A-Za-z0-9_]+)[ \t]*\\(",
		"\\1", "f,function,JavaScript functions", NULL);

/* the following matches headings with tags inside like
 * <h1><a href="#id109"><i>Some Text</i></a></h1>
 * and
 * <h1>Some Text</h1> */
#define SPACES "[ \t]*"
#define ATTRS "[^>]*"
#define INNER_HEADING \
	ATTRS ">" SPACES "(<" ATTRS ">" SPACES ")*([^<]+).*"

	addTagRegex (language,
		"<h1" INNER_HEADING "</h1>",
		"\\2", "n,namespace,H1 heading", "i");

	addTagRegex (language,
		"<h2" INNER_HEADING "</h2>",
		"\\2", "c,class,H2 heading", "i");

	addTagRegex (language,
		"<h3" INNER_HEADING "</h3>",
		"\\2", "v,variable,H3 heading", "i");
}

/* Create parser definition stucture */
extern parserDefinition* HtmlParser (void)
{
	static const char *const extensions [] = { "htm", "html", NULL };
	parserDefinition *const def = parserNew ("HTML");
	def->extensions = extensions;
	def->initialize = installHtmlRegex;
	def->regex      = TRUE;
	return def;
}

/* vi:set tabstop=4 shiftwidth=4: */
