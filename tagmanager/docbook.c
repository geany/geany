/*
*   $Id$
*
*   Copyright (c) 2001-2002, Darren Hiebert
*   Copyright (c) 2006, Enrico Tröger
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for the DocBook language,
*   to extract the id attribute from section and chapter tags.
*   The code is based on pascal.c from tagmanager.
*
*   TODO: skip commented blocks like <!-- <section id="comment">...</section> -->
*
*/

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <string.h>

#include "entry.h"
#include "parse.h"
#include "read.h"
#include "vstring.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
    K_SECTION,
    K_CHAPTER
} docbookKind;

static kindOption DocBookKinds [] = {
    { TRUE,  'c', "class",      "sections"},
    { TRUE,  'n', "namespace",  "chapters"},
};

/*
*   FUNCTION DEFINITIONS
*/

static void createDocBookTag (tagEntryInfo* const tag,
			     const vString* const name, const int kind)
{
    if (DocBookKinds[kind].enabled  &&  name != NULL  &&  vStringLength (name) > 0)
    {
        initTagEntry (tag, vStringValue (name));

        tag->kindName = DocBookKinds [kind].name;
        tag->kind     = DocBookKinds [kind].letter;
    }
    else
        initTagEntry (tag, NULL);
}

static void makeDocBookTag (const tagEntryInfo* const tag)
{
    if (tag->name != NULL) makeTagEntry(tag);
}

static const unsigned char* dbp;

#define starttoken(c) ((int) c == '"')
//#define intoken(c)    ((isalnum ((int) c) || isspace((int) c)) && (int) c != '"')
#define intoken(c)    ((isalnum ((int) c) || (int) c == '_') && (int) c != '"')
#define endtoken(c)   (! intoken (c))

static boolean tail (const char *cp)
{
    boolean result = FALSE;
    register int len = 0;

    while (*cp != '\0' && tolower ((int) *cp) == tolower ((int) dbp [len]))
	cp++, len++;
    if (*cp == '\0' && !intoken (dbp [len]))
    {
	dbp += len;
	result = TRUE;
    }
    return result;
}

/* Algorithm adapted from from GNU etags. */
static void findDocBookTags (void)
{
    vString *name = vStringNew ();
    tagEntryInfo tag;
    docbookKind kind = K_SECTION;
				/* each of these flags is TRUE iff: */
    boolean get_tagname = FALSE;/* point is after tag, so next item = potential tag */
    boolean found_tag = FALSE;	/* point is after a potential tag */
    boolean verify_tag = FALSE;

    dbp = fileReadLine ();

    while (dbp != NULL)
    {
	int c = *dbp++;

	if (c == '\0')		/* if end of line */
	{
	    dbp = fileReadLine ();
	    if (dbp == NULL  ||  *dbp == '\0')
		continue;
	    if (!((found_tag && verify_tag) || get_tagname))
		c = *dbp++;		/* only if don't need *dbp pointing
				    to the beginning of the name of the tag */
	}
	if ((c == '"' || c == '>') && found_tag) // end of proc or fn stmt
	{
	    verify_tag = TRUE;
		continue;
	}
	if (found_tag && verify_tag && *dbp != ' ')
	{
	    if (*dbp == '\0')
		continue;

	    if (found_tag && verify_tag) // not external proc, so make tag
	    {
			found_tag = FALSE;
			verify_tag = FALSE;
			makeDocBookTag(&tag);
			continue;
	    }
	}
	if (get_tagname)		/* grab name of proc or fn */
	{
	    const unsigned char *cp;

	    if (*dbp == '\0') continue;

	    /* grab block name */
	    while (isspace((int) *dbp)) ++dbp;
	    while ((int) *dbp != 'i') ++dbp;
	    while ((int) *dbp != 'd') ++dbp;
	    while ((int) *dbp != '"') ++dbp;
	    ++dbp;
	    for (cp = dbp ; *cp != '\0' && !endtoken(*cp);  cp++) continue;
	    vStringNCopyS(name, (const char*) dbp, cp - dbp);
	    createDocBookTag(&tag, name, kind);
	    //printf("%s\n", name);
	    dbp = cp;		/* set dbp to e-o-token */
	    get_tagname = FALSE;
	    found_tag = TRUE;
	}
	else if (!found_tag)
	{
	    switch (tolower ((int) c))
	    {
			case 's':
			{
				if (tail ("ection"))
				{
					get_tagname = TRUE;
					kind = K_SECTION;
				}
				break;
			}
			case 'c':
			{
				if (tail ("hapter"))
				{
					get_tagname = TRUE;
					kind = K_CHAPTER;
				}
				break;
			}
		}				/* while not eof */
    }
}
}

extern parserDefinition* DocBookParser (void)
{
    static const char *const extensions [] = { "d", "docbook", NULL };
    parserDefinition* def = parserNew ("Docbook");
    def->extensions = extensions;
    def->kinds      = DocBookKinds;
    def->kindCount  = KIND_COUNT (DocBookKinds);
    def->parser     = findDocBookTags;
    return def;
}

/* vi:set tabstop=8 shiftwidth=4: */
