/*
*   Copyright (c) 1996-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains debugging functions.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <glib.h>

#include "debug.h"

/* wrap g_warning so we don't include glib.h for all parsers, to keep compat with CTags */
void utils_warn(const char *msg)
{
	g_warning("%s", msg);
}
