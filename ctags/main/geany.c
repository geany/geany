/*
*   Copyright (c) 2016, Jiri Techet
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Defines external interface to option processing.
*/

#include "general.h"  /* must always come first */

#include "geany.h"
#include "vstring.h"

#include <string.h>
#include <glib.h>

/* tags_ignore is a NULL-terminated array of strings, read from ~/.config/geany/ignore.tags.
 * This file contains a space or newline separated list of symbols which should be ignored
 * by the C/C++ parser, see -I command line option of ctags for details. */
gchar **c_tags_ignore = NULL;

/*  Determines whether or not "name" should be ignored, per the ignore list.
 */
extern bool isIgnoreToken (const char *const name,
							  bool *const pIgnoreParens,
							  const char **const replacement)
{
	bool result = false;

	if (c_tags_ignore != NULL)
	{
		const size_t nameLen = strlen (name);
		unsigned int i;
		guint len = g_strv_length (c_tags_ignore);
		vString *token = vStringNew();

		if (pIgnoreParens != NULL)
			*pIgnoreParens = false;

		for (i = 0  ;  i < len ;  ++i)
		{
			size_t tokenLen;

			vStringCopyS (token, c_tags_ignore[i]);
			tokenLen = vStringLength (token);

			if (tokenLen >= 2 && vStringChar (token, tokenLen - 1) == '*' &&
				strncmp (vStringValue (token), name, tokenLen - 1) == 0)
			{
				result = true;
				break;
			}
			if (strncmp (vStringValue (token), name, nameLen) == 0)
			{
				if (nameLen == tokenLen)
				{
					result = true;
					break;
				}
				else if (tokenLen == nameLen + 1  &&
						vStringChar (token, tokenLen - 1) == '+')
				{
					result = true;
					if (pIgnoreParens != NULL)
						*pIgnoreParens = true;
					break;
				}
				else if (vStringChar (token, nameLen) == '=')
				{
					if (replacement != NULL)
						*replacement = vStringValue (token) + nameLen + 1;
					break;
				}
			}
		}
		vStringDelete (token);
	}
	return result;
}
