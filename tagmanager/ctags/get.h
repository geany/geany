/*
*   Copyright (c) 1998-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   External interface to get.c
*/
#ifndef _GET_H
#define _GET_H

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include "ctags.h"  /* to define langType */

/*
*   MACROS
*/
/*  Is the character valid as a character of a C identifier?
 *  VMS allows '$' in identifiers.
 */
#define isident(c)  (isalnum(c) || (c) == '_' || (c) == '$' || ((int) (c)) >= 0x80)

/*  Is the character valid as the first character of a C identifier?
 *  C++ allows '~' in destructors.
 *  VMS allows '$' in identifiers.
 *  Vala allows '@' in identifiers.
 */
#define isident1(c)  (isalpha(c) || (c) == '_' || (c) == '~' || (c) == '$' || (c) == '@' || ((int) (c)) >= 0x80)

/*
*   FUNCTION PROTOTYPES
*/
extern boolean isBraceFormat (void);
extern unsigned int getDirectiveNestLevel (void);
extern void cppInit (const boolean state, const boolean hasAtLiteralStrings,
                     const boolean hasCxxRawLiteralStrings);
extern void cppTerminate (void);
extern void cppBeginStatement (void);
extern void cppEndStatement (void);
extern void cppUngetc (const int c);
extern int cppGetc (void);
extern int skipOverCComment (void);
extern char *getArglistFromFilePos(MIOPos startPosition, const char *tokenName);
extern char *getArglistFromStr(char *buf, const char *name);

#endif  /* _GET_H */

/* vi:set tabstop=4 shiftwidth=4: */
