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
	boolean is_class;
};

struct NestingLevels
{
	NestingLevel *levels;
	int n;
	int allocated;
};

/*
*   FUNCTION PROTOTYPES
*/
NestingLevels *newNestingLevels(void);
void freeNestingLevels(NestingLevels *nls);
void addNestingLevel(NestingLevels *nls, int indentation,
	vString *name, boolean is_class);

#endif  /* _NESTLEVEL_H */

/* vi:set tabstop=4 shiftwidth=4: */
