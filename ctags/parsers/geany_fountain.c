/*
*   geany_fountain.c
*   Copyright (c) 2021, xiota
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for Fountain screenplays.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <string.h>

#include "entry.h"
#include "parse.h"
#include "read.h"
#include "routines.h"
#include "vstring.h"

/*
*   DATA DEFINITIONS
*/

static kindDefinition FountainKinds[] = {
	{ true, 'v', "Scene", "Scenes" }
};

/*
*   FUNCTION DEFINITIONS
*/

static void makeFountainTag (const vString* const name)
{
	tagEntryInfo e;
	initTagEntry (&e, vStringValue(name), 0);
	makeTagEntry(&e);
}


static void findFountainTags (void)
{
	vString *name = vStringNew();
	const unsigned char *line;

	while ((line = readLineFromInputFile()) != NULL) {
		int len = strlen(line);

		if (len < 2) {
			continue;
		}

		if (len > 1 && line[0] == '.' && line[1] != '.') {
			vStringClear(name);
			vStringCatS(name, (const char *) line);
			makeFountainTag(name);
		}
		else if (
			strncmp(line, "INT. ", 5) == 0 || strncmp(line, "INT ", 4) == 0 ||
			strncmp(line, "EXT. ", 5) == 0 || strncmp(line, "EXT ", 4) == 0 ||
			strncmp(line, "EST. ", 5) == 0 || strncmp(line, "EST ", 4) == 0 ||
			strncmp(line, "I/E. ", 5) == 0 || strncmp(line, "I/E ", 4) == 0 ||
			strncmp(line, "E/I. ", 5) == 0 || strncmp(line, "E/I ", 4) == 0 ||
			strncmp(line, "INT./EXT. ", 10) == 0 || strncmp(line, "INT/EXT ", 8) == 0 ||
			strncmp(line, "EXT./INT. ", 10) == 0 || strncmp(line, "EXT/INT ", 8) == 0 ||
			strncmp(line, "INT/EXT. ", 9) == 0 || strncmp(line, "EXT/INT. ", 9) == 0
		) {
			vStringClear(name);
			vStringCatS(name, (const char *) line);
			makeFountainTag(name);
		}
	}

	vStringDelete (name);
}

extern parserDefinition* FountainParser (void)
{
	static const char *const patterns [] = { "*.fountain", "*.spmd", NULL };
	static const char *const extensions [] = { "fountain", "spmd", NULL };
	parserDefinition* const def = parserNew ("Fountain");

	def->kindTable = FountainKinds;
	def->kindCount = ARRAY_SIZE (FountainKinds);
	def->patterns = patterns;
	def->extensions = extensions;
	def->parser = findFountainTags;
	return def;
}
