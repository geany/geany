/*
*   Copyright (c) 2016, Jiri Techet
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Defines ctags API when compiled as a library.
*/
#ifndef TM_CTAGS_H
#define TM_CTAGS_H

#include <stdlib.h>
#include <stdbool.h>

#include "tm_source_file.h"

extern void ctagsInit(void);
extern void ctagsParse(unsigned char *buffer, size_t bufferSize,
	const char *fileName, const int language, TMSourceFile *source_file);
extern const char *ctagsGetLangName(int lang);
extern int ctagsGetNamedLang(const char *name);
extern const char *ctagsGetLangKinds(int lang);
extern const char *ctagsGetKindName(char kind, int lang);
extern char ctagsGetKindFromName(const char *name, int lang);
extern unsigned int ctagsGetLangCount(void);

#endif /* TM_CTAGS_H */
