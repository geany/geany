/*
*   Copyright (c) 2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   External interface to routines.c
*/
#ifndef CTAGS_MAIN_ROUTINES_H
#define CTAGS_MAIN_ROUTINES_H

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <stdio.h>

#include "mio.h"

/*
*   MACROS
*/
#define ARRAY_SIZE(X)      (sizeof (X) / sizeof (X[0]))

#define STRINGIFY(X) STRINGIFY_(X)
#define STRINGIFY_(X) #X

#endif  /* CTAGS_MAIN_ROUTINES_H */

/* vi:set tabstop=4 shiftwidth=4: */
