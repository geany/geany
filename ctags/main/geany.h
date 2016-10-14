/*
*   Copyright (c) 2016, Jiri Techet
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Defines external interface to option processing.
*/
#ifndef CTAGS_GEANY_H
#define CTAGS_GEANY_H

#include "types.h"

typedef bool (*tagEntryFunction) (const tagEntryInfo *const tag, void *userData);
typedef bool (*passStartCallback) (void *userData);


extern bool isIgnoreToken (const char *const name, bool *const pIgnoreParens, const char **const replacement);

extern void setTagEntryFunction(tagEntryFunction entry_function, void *user_data);
extern int callTagEntryFunction(const tagEntryInfo *const tag);

#endif  /* CTAGS_GEANY_H */
