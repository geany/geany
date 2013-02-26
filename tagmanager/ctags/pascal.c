/*
*   Copyright (c) 2001-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for the Pascal language,
*   including some extensions for Object Pascal.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <string.h>

#include "entry.h"
#include "parse.h"
#include "read.h"
#include "main.h"
#include "vstring.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
    K_FUNCTION, K_PROCEDURE
} pascalKind;

static kindOption PascalKinds [] = {
    { TRUE, 'f', "function", "functions"},
    { TRUE, 'f', "function", "procedures"}
};

/*
*   FUNCTION DEFINITIONS
*/

static void createPascalTag (tagEntryInfo* const tag,
			     const vString* const name, const int kind,
			     const char *arglist, const char *vartype)
{
    if (PascalKinds [kind].enabled  &&  name != NULL  &&  vStringLength (name) > 0)
    {
        initTagEntry (tag, vStringValue (name));

        tag->kindName = PascalKinds [kind].name;
        tag->kind     = PascalKinds [kind].letter;
        tag->extensionFields.arglist = arglist;
        tag->extensionFields.varType = vartype;
    }
    else
        initTagEntry (tag, NULL);
}

static void makePascalTag (const tagEntryInfo* const tag)
{
    if (tag->name != NULL)
	makeTagEntry (tag);
}

static const unsigned char* dbp;

#define starttoken(c) (isalpha ((int) c) || (int) c == '_')
#define intoken(c)    (isalnum ((int) c) || (int) c == '_' || (int) c == '.')
#define endtoken(c)   (! intoken (c)  &&  ! isdigit ((int) c))

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

static void parseArglist(const char *buf, char **arglist, char **vartype)
{
    char *c, *start, *end;
    int level;

    if (NULL == buf || NULL == arglist)
	return;

    c = strdup(buf);
    /* parse argument list which can be missing like in "function ginit:integer;" */
    if (NULL != (start = strchr(c, '(')))
    {
	for (level = 1, end = start + 1; level > 0; ++end)
	{
	    if ('\0' == *end)
		break;
	    else if ('(' == *end)
		++ level;
	    else if (')' == *end)
		-- level;
	}
    }
    else /* if no argument list was found, continue looking for a return value */
    {
	start = "()";
	end = c;
    }

    /* parse return type if requested by passing a non-NULL vartype argument */
    if (NULL != vartype)
    {
	char *var, *var_start;

	*vartype = NULL;

	if (NULL != (var = strchr(end, ':')))
	{
	    var++; /* skip ':' */
	    while (isspace((int) *var))
		++var;

	    if (starttoken(*var))
	    {
		var_start = var;
		var++;
		while (intoken(*var))
		    var++;
		if (endtoken(*var))
		{
		    *var = '\0';
		    *vartype = strdup(var_start);
		}
	    }
	}
    }

    *end = '\0';
    *arglist = strdup(start);

    eFree(c);
}


/* Algorithm adapted from from GNU etags.
 * Locates tags for procedures & functions.  Doesn't do any type- or
 * var-definitions.  It does look for the keyword "extern" or "forward"
 * immediately following the procedure statement; if found, the tag is
 * skipped.
 */
static void findPascalTags (void)
{
    vString *name = vStringNew ();
    tagEntryInfo tag;
    char *arglist = NULL;
    char *vartype = NULL;
    pascalKind kind = K_FUNCTION;
				/* each of these flags is TRUE iff: */
    boolean incomment = FALSE;	/* point is inside a comment */
    int comment_char = '\0';    /* type of current comment */
    boolean inquote = FALSE;	/* point is inside '..' string */
    boolean get_tagname = FALSE;/* point is after PROCEDURE/FUNCTION
				    keyword, so next item = potential tag */
    boolean found_tag = FALSE;	/* point is after a potential tag */
    boolean inparms = FALSE;	/* point is within parameter-list */
    boolean verify_tag = FALSE;	/* point has passed the parm-list, so the
				   next token will determine whether this
				   is a FORWARD/EXTERN to be ignored, or
				   whether it is a real tag */

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
				    to the beginning of the name of
				    the procedure or function */
	}
	if (incomment)
	{
	    if (comment_char == '{' && c == '}')
		incomment = FALSE;
	    else if (comment_char == '(' && c == '*' && *dbp == ')')
	    {
		dbp++;
		incomment = FALSE;
	    }
	    continue;
	}
	else if (inquote)
	{
	    if (c == '\'')
		inquote = FALSE;
	    continue;
	}
	else switch (c)
	{
	    case '\'':
		inquote = TRUE;	/* found first quote */
		continue;
	    case '{':		/* found open { comment */
		incomment = TRUE;
		comment_char = c;
		continue;
	    case '(':
		if (*dbp == '*')	/* found open (* comment */
		{
		    incomment = TRUE;
		    comment_char = c;
		    dbp++;
		}
		else if (found_tag)  /* found '(' after tag, i.e., parm-list */
		    inparms = TRUE;
		continue;
	    case ')':		/* end of parms list */
		if (inparms)
		    inparms = FALSE;
		continue;
	    case ';':
		if (found_tag && !inparms) /* end of proc or fn stmt */
		{
		    verify_tag = TRUE;
		    break;
		}
		continue;
	}
	if (found_tag && verify_tag && *dbp != ' ')
	{
	    /* check if this is an "extern" declaration */
	    if (*dbp == '\0')
		continue;
	    if (tolower ((int) *dbp == 'e'))
	    {
		if (tail ("extern"))	/* superfluous, really! */
		{
		    found_tag = FALSE;
		    verify_tag = FALSE;
		}
	    }
	    else if (tolower ((int) *dbp) == 'f')
	    {
		if (tail ("forward"))	/*  check for forward reference */
		{
		    found_tag = FALSE;
		    verify_tag = FALSE;
		}
	    }
	    else if (tolower ((int) *dbp) == 't')
	    {
		if (tail ("type"))	/*  check for forward reference */
		{
		    found_tag = FALSE;
		    verify_tag = FALSE;
		}
	    }
	    if (found_tag && verify_tag) /* not external proc, so make tag */
	    {
		found_tag = FALSE;
		verify_tag = FALSE;
		makePascalTag (&tag);
		continue;
	    }
	}
	if (get_tagname)		/* grab name of proc or fn */
	{
	    const unsigned char *cp;

	    if (*dbp == '\0')
		continue;

	    /* grab block name */
	    while (isspace ((int) *dbp))
		++dbp;
	    for (cp = dbp  ;  *cp != '\0' && !endtoken (*cp)  ;  cp++)
		continue;
	    vStringNCopyS (name, (const char*) dbp,  cp - dbp);
	    if (arglist != NULL)
		eFree(arglist);
	    if (kind == K_FUNCTION && vartype != NULL)
		eFree(vartype);
	    parseArglist((const char*) cp, &arglist, (kind == K_FUNCTION) ? &vartype : NULL);
	    createPascalTag (&tag, name, kind, arglist, (kind == K_FUNCTION) ? vartype : NULL);
	    dbp = cp;		/* set dbp to e-o-token */
	    get_tagname = FALSE;
	    found_tag = TRUE;
	    /* and proceed to check for "extern" */
	}
	else if (!incomment && !inquote && !found_tag)
	{
	    switch (tolower ((int) c))
	    {
		case 'c':
		    if (tail ("onstructor"))
		    {
			get_tagname = TRUE;
			kind = K_PROCEDURE;
		    }
		    break;
		case 'd':
		    if (tail ("estructor"))
		    {
			get_tagname = TRUE;
			kind = K_PROCEDURE;
		    }
		    break;
		case 'p':
		    if (tail ("rocedure"))
		    {
			get_tagname = TRUE;
			kind = K_PROCEDURE;
		    }
		    break;
		case 'f':
		    if (tail ("unction"))
		    {
			get_tagname = TRUE;
			kind = K_FUNCTION;
		    }
		    break;
		case 't':
		    if (tail ("ype"))
		    {
			get_tagname = TRUE;
			kind = K_FUNCTION;
		    }
		    break;
	    }
	}  /* while not eof */
    }
    if (arglist != NULL)
	eFree(arglist);
    if (vartype != NULL)
	eFree(vartype);
    vStringDelete(name);
}

extern parserDefinition* PascalParser (void)
{
    static const char *const extensions [] = { "p", "pas", NULL };
    parserDefinition* def = parserNew ("Pascal");
    def->extensions = extensions;
    def->kinds      = PascalKinds;
    def->kindCount  = KIND_COUNT (PascalKinds);
    def->parser     = findPascalTags;
    return def;
}

/* vi:set tabstop=8 shiftwidth=4: */
