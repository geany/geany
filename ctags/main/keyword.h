/*
*   Copyright (c) 1998-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   External interface to keyword.c
*/
#ifndef CTAGS_MAIN_KEYWORD_H
#define CTAGS_MAIN_KEYWORD_H

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */
#include "types.h"

/*
*   MACROS
*/
#define KEYWORD_NONE -1

/*
*   FUNCTION PROTOTYPES
*/
extern void addKeyword (const char *const string, langType language, int value);
extern int lookupKeyword (const char *const string, langType language);
extern int lookupCaseKeyword (const char *const string, langType language);

/*
* KEYWORD GROUP API: Adding keywords for value in batch
*/
struct keywordGroup {
	int value;
	bool addingUnlessExisting;
	const char *keywords []; 	/* NULL terminated */
};

extern void addKeywordGroup (const struct keywordGroup *const groupdef,
							 langType language);

/*
* DIALECTAL KEYWROD API
*/
#define MAX_DIALECTS 10
struct dialectalKeyword {
	const char *keyword;
	int value;

	/* If this keyword is valid for Nth dialect,
	   isValid[N] should be 1; else 0. */
	bool isValid [MAX_DIALECTS];
};

extern void addDialectalKeywords (const struct dialectalKeyword *const dkeywords,
								  size_t nDkeyword,
								  langType language,
								  const char **dialectMap,
								  size_t nMap);

#endif  /* CTAGS_MAIN_KEYWORD_H */
