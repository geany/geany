/*
*   $Id$
*
*   Copyright (c) 1999-2002, Darren Hiebert
*   Copyright 2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   Defines external interface to scope nesting levels for tags.
*/
#ifndef _NESTLEVEL_H
#define _NESTLEVEL_H

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include "vstring.h"

/*
*   DATA DECLARATIONS
*/
typedef struct NestingLevel NestingLevel;
typedef struct NestingLevels NestingLevels;

struct NestingLevel
{
	int indentation;
	vString *name;
	int type;
	boolean is_class;		/* should be replaced by type field */
};

struct NestingLevels
{
	NestingLevel *levels;
	int n;					/* number of levels in use */
	int allocated;
};

/*
*   FUNCTION PROTOTYPES
*/
extern NestingLevels *newNestingLevels(void);
extern void freeNestingLevels(NestingLevels *nls);
extern void addNestingLevel(NestingLevels *nls, int indentation,
	const vString *name, boolean is_class);
extern void nestingLevelsPush(NestingLevels *nls,
	const vString *name, int type);
extern void nestingLevelsPop(NestingLevels *nls);
extern NestingLevel *nestingLevelsGetCurrent(NestingLevels *nls);

#endif  /* _NESTLEVEL_H */

/* vi:set tabstop=4 shiftwidth=4: */
