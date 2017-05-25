/*
*   Copyright (c) 1999-2002, Darren Hiebert
*   Copyright 2009-2011 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Defines external interface to scope nesting levels for tags.
*/
#ifndef CTAGS_MAIN_NESTLEVEL_H
#define CTAGS_MAIN_NESTLEVEL_H

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
extern NestingLevels *nestingLevelsNew(void);
extern void nestingLevelsFree(NestingLevels *nls);
extern void nestingLevelsPush(NestingLevels *nls,
	const vString *name, int type);
extern void nestingLevelsPop(NestingLevels *nls);
extern NestingLevel *nestingLevelsGetCurrent(NestingLevels *nls);

#endif  /* CTAGS_MAIN_NESTLEVEL_H */
