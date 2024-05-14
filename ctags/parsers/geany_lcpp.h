/*
*   Copyright (c) 1998-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   External interface to get.c
*/
#ifndef CTAGS_MAIN_GET_H
#define CTAGS_MAIN_GET_H

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */
#include "types.h"

/*
 *   DATA DECLARATIONS
 */

enum eCharacters {
	/* white space characters */
	SPACE         = ' ',
	NEWLINE       = '\n',
	CRETURN       = '\r',
	FORMFEED      = '\f',
	TAB           = '\t',
	VTAB          = '\v',

	/* some hard to read characters */
	DOUBLE_QUOTE  = '"',
	SINGLE_QUOTE  = '\'',
	BACKSLASH     = '\\',

	/* symbolic representations, above 0xFF not to conflict with any byte */
	STRING_SYMBOL = ('S' + 0xff),
	CHAR_SYMBOL   = ('C' + 0xff)
};
 
/*
*   MACROS
*/
/*  Is the character valid as a character of a C identifier?
 *  VMS allows '$' in identifiers.
 */
#define lcppIsident(c)  (isalnum(c) || (c) == '_' || (c) == '$')

/*  Is the character valid as the first character of a C identifier?
 *  C++ allows '~' in destructors.
 *  VMS allows '$' in identifiers.
 *  Vala allows '@' in identifiers.
 */
#define lcppIsident1(c)  ( ((c >= 0) && (c < 0x80) && isalpha(c)) \
		        || (c) == '_' || (c) == '~' || (c) == '$' || (c) == '@')
/* NOTE about isident1 profitability

   Doing the same as isascii before passing value to isalpha
   ----------------------------------------------------------
   cppGetc() can return the value out of range of char.
   cppGetc calls skipToEndOfString and skipToEndOfString can
   return STRING_SYMBOL(== 338).

   Depending on the platform, isalpha(338) returns different value .
   As far as Fedora22, it returns 0. On Windows 2010, it returns 1.

   man page on Fedora 22 says:

       These functions check whether c, which must have the value of an
       unsigned char or EOF, falls into a certain character class
       according to the specified locale.

   isascii is for suitable to verify the range of input. However, it
   is not portable enough. */

#define RoleTemplateUndef { true, "undef", "undefined" }

#define RoleTemplateSystem { true, "system", "system header" }
#define RoleTemplateLocal  { true, "local", "local header" }

/*
*   FUNCTION PROTOTYPES
*/
extern bool lcppIsBraceFormat (void);
extern unsigned int lcppGetDirectiveNestLevel (void);

extern void lcppInit (const bool state,
		     const bool hasAtLiteralStrings,
		     const bool hasCxxRawLiteralStrings,
		     int defineMacroKindIndex);
extern void lcppTerminate (void);
extern void lcppBeginStatement (void);
extern void lcppEndStatement (void);
extern void lcppUngetc (const int c);
extern int lcppGetc (void);
extern int lcppSkipOverCComment (void);

extern char *lcppGetSignature (void);
extern void lcppStartCollectingSignature (void);
extern void lcppStopCollectingSignature (void);
extern void lcppClearSignature (void);

extern bool cppIsIgnoreToken (const char *const name,
			      bool *const pIgnoreParens,
			      const char **const replacement);

#endif  /* CTAGS_MAIN_GET_H */
