/*
*   Copyright (c) 1999-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Defines external interface to resizable string lists.
*/
#ifndef CTAGS_MAIN_STRLIST_H
#define CTAGS_MAIN_STRLIST_H

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include "vstring.h"
#include "ptrarray.h"

#include <stdio.h>

/*
*   DATA DECLARATIONS
*/
typedef ptrArray stringList;

/*
*   FUNCTION PROTOTYPES
*/
extern stringList *stringListNew (void);
extern void stringListAdd (stringList *const current, vString *string);
extern void stringListRemoveLast (stringList *const current);
extern void stringListCombine (stringList *const current, stringList *const from);
extern stringList* stringListNewFromArgv (const char* const* const list);
extern stringList* stringListNewFromFile (const char* const fileName);
extern void stringListClear (stringList *const current);
extern unsigned int stringListCount (const stringList *const current);
extern vString* stringListItem (const stringList *const current, const unsigned int indx);
extern vString* stringListLast (const stringList *const current);
extern void stringListDelete (stringList *const current);
extern bool stringListHasInsensitive (const stringList *const current, const char *const string);
extern bool stringListHas (const stringList *const current, const char *const string);
extern bool stringListHasTest (const stringList *const current,
				  bool (*test)(const char *s, void *userData),
				  void *userData);
extern bool stringListDeleteItemExtension (stringList* const current, const char* const extension);
extern bool stringListExtensionMatched (const stringList* const list, const char* const extension);
extern vString* stringListExtensionFinds (const stringList* const list, const char* const extension);
extern bool stringListFileMatched (const stringList* const list, const char* const str);
extern vString* stringListFileFinds (const stringList* const list, const char* const str);
extern void stringListPrint (const stringList *const current, FILE *fp);
extern void stringListReverse (const stringList *const current);

extern stringList *stringListNewBySplittingWordIntoSubwords (const char* originalWord);

#endif  /* CTAGS_MAIN_STRLIST_H */
