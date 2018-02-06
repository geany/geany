/*
*   Copyright (c) 1998-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   External interface to sort.c
*/
#ifndef CTAGS_MAIN_SORT_H
#define CTAGS_MAIN_SORT_H

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

/*
*   FUNCTION PROTOTYPES
*/
extern void catFile (const char *const name);

#ifdef EXTERNAL_SORT
extern void externalSortTags (const bool toStdout);
#else
extern void internalSortTags (const bool toStdout);
#endif

#endif  /* CTAGS_MAIN_SORT_H */
