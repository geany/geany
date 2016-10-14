/*
*   Copyright (c) 2016, Jiri Techet
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Defines ctags API when compiled as a library.
*/
#ifndef CTAGS_API_H
#define CTAGS_API_H

#ifdef CTAGS_LIB

typedef bool (*tagEntryFunction) (const tagEntryInfo *const tag, void *userData);
typedef bool (*passStartCallback) (void *userData);

#endif /* CTAGS_LIB */

#endif  /* CTAGS_API_H */
