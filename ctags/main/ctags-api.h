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

#include "general.h"  /* must always come first */

#ifdef GEANY_CTAGS_LIB

#include <stdlib.h>
#include <stdbool.h>

typedef struct {
	const char *name;
	const char *signature;
	const char *scopeName;
	const char *inheritance;
	const char *varType;
	const char *access;
	const char *implementation;
	char kindLetter;
	bool isFileScope;
	unsigned long lineNumber;
	int lang;
} ctagsTag;

/* Callback invoked for every tag found by the parser. The return value is
 * currently unused. */
typedef bool (*tagEntryFunction) (const ctagsTag *const tag, void *userData);

/* Callback invoked when rescan fails telling us how many of the passed tags are valid */
typedef void (*rescanFailedFunction) (unsigned long validTagNum, void *userData);


extern void ctagsInit(void);
extern void ctagsParse(unsigned char *buffer, size_t bufferSize,
	const char *fileName, const int language,
	tagEntryFunction tagCallback, rescanFailedFunction rescanCallback,
	void *userData);
extern const char *ctagsGetLangName(int lang);
extern int ctagsGetNamedLang(const char *name);
extern const char *ctagsGetLangKinds(int lang);
extern const char *ctagsGetKindName(char kind, int lang);
extern char ctagsGetKindFromName(const char *name, int lang);
extern unsigned int ctagsGetLangCount(void);

#endif /* GEANY_CTAGS_LIB */

#endif  /* CTAGS_API_H */
