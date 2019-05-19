/*
 *
 *  Copyright (c) 2015, Red Hat, Inc.
 *  Copyright (c) 2015, Masatake YAMATO
 *
 *  Author: Masatake YAMATO <yamato@redhat.com>
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License version 2 or (at your option) any later version.
 *
 */
#ifndef CTAGS_MAIN_XTAG_PRIVATE_H
#define CTAGS_MAIN_XTAG_PRIVATE_H

/*
*   INCLUDE FILES
*/

#include "general.h"

#include "colprint_p.h"

/*
*   FUNCTION PROTOTYPES
*/

extern xtagDefinition* getXtagDefinition (xtagType type);
extern xtagType  getXtagTypeForLetter (char letter);
extern xtagType  getXtagTypeForNameAndLanguage (const char *name, langType language);

extern bool enableXtag (xtagType type, bool state);
extern bool isXtagFixed (xtagType type);
extern bool isCommonXtag (xtagType type);
extern int  getXtagOwner (xtagType type);

const char* getXtagName (xtagType type);

extern void initXtagObjects (void);
extern int countXtags (void);

extern int defineXtag (xtagDefinition *def, langType language);
extern xtagType nextSiblingXtag (xtagType type);

/* --list-extras implementation. LANGUAGE must be initialized. */
extern struct colprintTable * xtagColprintTableNew (void);
extern void xtagColprintAddCommonLines (struct colprintTable *table);
extern void xtagColprintAddLanguageLines (struct colprintTable *table, langType language);
extern void xtagColprintTablePrint (struct colprintTable *table,
									bool withListHeader, bool machinable, FILE *fp);

#endif	/* CTAGS_MAIN_FIELD_PRIVATE_H */
