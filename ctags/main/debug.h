/*
*   Copyright (c) 1998-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   External interface to debug.c
*/
#ifndef CTAGS_MAIN_DEBUG_H
#define CTAGS_MAIN_DEBUG_H

/*
*   Include files
*/
#include "general.h"  /* must always come first */

#include "entry.h"

/*
*   Macros
*/

/* fake debug statement macro */
#define DebugStatement(x)      ;
#define PrintStatus(x)         ;
/* wrap g_warning so we don't include glib.h for all parsers, to keep compat with CTags */
extern void utils_warn(const char *msg);
#define Assert(x) if (!(x)) utils_warn("Assert(" #x ") failed!")
#define AssertNotReached() Assert(!"The control reaches unexpected place")

#endif  /* CTAGS_MAIN_DEBUG_H */
