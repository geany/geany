/******************************************************************
 *    LexHaskell.cxx
 *
 *    A haskell lexer for the scintilla code control.
 *    Some stuff "lended" from LexPython.cxx and LexCPP.cxx.
 *    External lexer stuff inspired from the caml external lexer.
 *
 *    Written by Tobias Engvall - tumm at dtek dot chalmers dot se
 *
 *    Several bug fixes by Krasimir Angelov - kr.angelov at gmail.com
 *
 *    Improvements by kudah - kudahkukarek at gmail.com
 *
 *    TODO:
 *    * Implement a folder :)
 *    * Nice Character-lexing (stuff inside '\''), LexPython has
 *      this.
 *
 *
 *****************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "PropSetSimple.h"
#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"

#ifdef SCI_NAMESPACE
using namespace Scintilla;
#endif

#ifdef BUILD_AS_EXTERNAL_LEXER

#include "ExternalLexer.h"
#include "WindowAccessor.h"

#define BUILD_EXTERNAL_LEXER 0

#endif

#define HA_MODE_DEFAULT     0
#define HA_MODE_IMPORT1     1
#define HA_MODE_IMPORT2     2
#define HA_MODE_IMPORT3     3
#define HA_MODE_MODULE      4
#define HA_MODE_FFI         5
#define HA_MODE_TYPE        6

static inline bool IsAWordStart(const int ch) {
   return (IsLowerCase(ch) || IsUpperCase(ch) || ch == '_');
}

static inline bool IsAWordChar(const int ch, const bool magicHash) {
   return (  IsAlphaNumeric(ch)
          || ch == '_'
          || ch == '\''
          || (magicHash && ch == '#'));
}

static inline bool IsAnOperatorChar(const int ch) {
   return
      (  ch == '!' || ch == '#' || ch == '$' || ch == '%'
      || ch == '&' || ch == '*' || ch == '+' || ch == '-'
      || ch == '.' || ch == '/' || ch == ':' || ch == '<'
      || ch == '=' || ch == '>' || ch == '?' || ch == '@'
      || ch == '\\' || ch == '^' || ch == '|' || ch == '~');
}

static void ColorizeHaskellDoc(unsigned int startPos, int length, int initStyle,
                               WordList *keywordlists[], Accessor &styler) {

   WordList &keywords = *keywordlists[0];
   WordList &ffi      = *keywordlists[1];

   // property lexer.haskell.allow.hash
   //  Set to 1 to allow the # character in identifiers with the haskell lexer.
   //  (GHC -XMagicHash extension)
   const bool magicHash = styler.GetPropertyInt("lexer.haskell.allow.hash") != 0;
   const bool stylingWithinPreprocessor = styler.GetPropertyInt("styling.within.preprocessor") != 0;

   StyleContext sc(startPos, length, initStyle, styler);

   int lineCurrent = styler.GetLine(startPos);
   int state = lineCurrent ? styler.GetLineState(lineCurrent-1)
                           : HA_MODE_DEFAULT;
   int mode  = state & 0xF;
   int xmode = state >> 4; // obscure parameter. Means different things in different modes.

   while (sc.More()) {
      // Check for state end

         // Operator
      if (sc.state == SCE_HA_OPERATOR) {
         int style = SCE_HA_OPERATOR;

         if (sc.ch == ':' &&
            // except "::"
            !(sc.chNext == ':' && !IsAnOperatorChar(sc.GetRelative(2)))) {
            style = SCE_HA_CAPITAL;
         }

         while(IsAnOperatorChar(sc.ch))
               sc.Forward();

         styler.ColourTo(sc.currentPos - 1, style);
         sc.ChangeState(SCE_HA_DEFAULT);
      }
         // String
      else if (sc.state == SCE_HA_STRING) {
         if (sc.ch == '\"') {
            sc.Forward();
            sc.SetState(SCE_HA_DEFAULT);
         } else if (sc.ch == '\\') {
            sc.Forward(2);
         } else if (sc.atLineEnd) {
            sc.SetState(SCE_HA_DEFAULT);
         } else {
            sc.Forward();
         }
      }
         // Char
      else if (sc.state == SCE_HA_CHARACTER) {
         if (sc.ch == '\'') {
            sc.Forward();
            sc.SetState(SCE_HA_DEFAULT);
         } else if (sc.ch == '\\') {
            sc.Forward(2);
         } else if (sc.atLineEnd) {
            sc.SetState(SCE_HA_DEFAULT);
         } else {
            sc.Forward();
         }
      }
         // Number
      else if (sc.state == SCE_HA_NUMBER) {
         if (IsADigit(sc.ch, xmode) ||
            (sc.ch=='.' && IsADigit(sc.chNext, xmode))) {
            sc.Forward();
         } else if ((xmode == 10) &&
                    (sc.ch == 'e' || sc.ch == 'E') &&
                    (IsADigit(sc.chNext) || sc.chNext == '+' || sc.chNext == '-')) {
            sc.Forward();
            if (sc.ch == '+' || sc.ch == '-')
                sc.Forward();
         } else {
            sc.SetState(SCE_HA_DEFAULT);
         }
      }
         // Keyword or Identifier
      else if (sc.state == SCE_HA_IDENTIFIER) {
         while (sc.More()) {
            if (IsAWordChar(sc.ch, magicHash)) {
               sc.Forward();
            } else if (xmode == SCE_HA_CAPITAL && sc.ch=='.') {
               if (isupper(sc.chNext)) {
                  xmode = SCE_HA_CAPITAL;
                  sc.Forward();
               } else if (IsAWordStart(sc.chNext)) {
                  xmode = SCE_HA_IDENTIFIER;
                  sc.Forward();
               } else if (IsAnOperatorChar(sc.chNext)) {
                  xmode = SCE_HA_OPERATOR;
                  sc.Forward();
               } else {
                  break;
               }
            } else if (xmode == SCE_HA_OPERATOR && IsAnOperatorChar(sc.ch)) {
               sc.Forward();
            } else {
               break;
            }
         }

         char s[100];
         sc.GetCurrent(s, sizeof(s));

         int style = xmode;

         int new_mode = HA_MODE_DEFAULT;

         if (keywords.InList(s)) {
            style = SCE_HA_KEYWORD;
         } else if (isupper(s[0])) {
            if (mode >= HA_MODE_IMPORT1 && mode <= HA_MODE_IMPORT3) {
               style    = SCE_HA_MODULE;
               new_mode = HA_MODE_IMPORT2;
            } else if (mode == HA_MODE_MODULE) {
               style = SCE_HA_MODULE;
            }
         } else if (mode == HA_MODE_IMPORT1 &&
                    strcmp(s,"qualified") == 0) {
             style    = SCE_HA_KEYWORD;
             new_mode = HA_MODE_IMPORT1;
         } else if (mode == HA_MODE_IMPORT2) {
             if (strcmp(s,"as") == 0) {
                style    = SCE_HA_KEYWORD;
                new_mode = HA_MODE_IMPORT3;
            } else if (strcmp(s,"hiding") == 0) {
                style     = SCE_HA_KEYWORD;
            }
         } else if (mode == HA_MODE_TYPE) {
            if (strcmp(s,"family") == 0)
               style    = SCE_HA_KEYWORD;
         }

         if (mode == HA_MODE_FFI) {
            if (ffi.InList(s)) {
               style = SCE_HA_KEYWORD;
               new_mode = HA_MODE_FFI;
            }
         }

         styler.ColourTo(sc.currentPos - 1, style);

         if (strcmp(s,"import") == 0 && mode != HA_MODE_FFI)
            new_mode = HA_MODE_IMPORT1;
         else if (strcmp(s,"module") == 0)
            new_mode = HA_MODE_MODULE;
         else if (strcmp(s,"foreign") == 0)
            new_mode = HA_MODE_FFI;
         else if (strcmp(s,"type") == 0
               || strcmp(s,"data") == 0)
            new_mode = HA_MODE_TYPE;

         xmode = 0;
         sc.ChangeState(SCE_HA_DEFAULT);
         mode = new_mode;
      }

         // Comments
            // Oneliner
      else if (sc.state == SCE_HA_COMMENTLINE) {
         if (xmode == 1 && sc.ch != '-') {
            xmode = 0;
            if (IsAnOperatorChar(sc.ch))
               sc.ChangeState(SCE_HA_OPERATOR);
         } else if (sc.atLineEnd) {
            sc.SetState(SCE_HA_DEFAULT);
         } else {
            sc.Forward();
         }
      }
            // Nested
      else if (sc.state == SCE_HA_COMMENTBLOCK) {
         if (sc.Match('{','-')) {
            sc.Forward(2);
            xmode++;
         }
         else if (sc.Match('-','}')) {
            sc.Forward(2);
            xmode--;
            if (xmode == 0) {
               sc.SetState(SCE_HA_DEFAULT);
            }
         } else {
            if (sc.atLineEnd) {
                // Remember the line state for future incremental lexing
                styler.SetLineState(lineCurrent, (xmode << 4) | mode);
                lineCurrent++;
            }
            sc.Forward();
         }
      }
            // Pragma
      else if (sc.state == SCE_HA_PRAGMA) {
         if (sc.Match("#-}")) {
            sc.Forward(3);
            sc.SetState(SCE_HA_DEFAULT);
         } else {
            sc.Forward();
         }
      }
            // Preprocessor
      else if (sc.state == SCE_HA_PREPROCESSOR) {
         if (stylingWithinPreprocessor && !IsAWordStart(sc.ch)) {
            sc.SetState(SCE_HA_DEFAULT);
         } else if (sc.ch == '\\' && !stylingWithinPreprocessor) {
            sc.Forward(2);
         } else if (sc.atLineEnd) {
            sc.SetState(SCE_HA_DEFAULT);
         } else {
            sc.Forward();
         }
      }
      // New state?
      if (sc.state == SCE_HA_DEFAULT) {
         // Digit
         if (IsADigit(sc.ch)) {
            sc.SetState(SCE_HA_NUMBER);
            if (sc.ch == '0' && (sc.chNext == 'X' || sc.chNext == 'x')) {
                // Match anything starting with "0x" or "0X", too
                sc.Forward(2);
                xmode = 16;
            } else if (sc.ch == '0' && (sc.chNext == 'O' || sc.chNext == 'o')) {
                // Match anything starting with "0x" or "0X", too
                sc.Forward(2);
                xmode = 8;
            } else {
                sc.Forward();
                xmode = 10;
            }
            mode = HA_MODE_DEFAULT;
         }
         // Pragma
         else if (sc.Match("{-#")) {
            sc.SetState(SCE_HA_PRAGMA);
            sc.Forward(3);
         }
         // Comment line
         else if (sc.Match('-','-')) {
            sc.SetState(SCE_HA_COMMENTLINE);
            sc.Forward(2);
            xmode = 1;
         }
         // Comment block
         else if (sc.Match('{','-')) {
            sc.SetState(SCE_HA_COMMENTBLOCK);
            sc.Forward(2);
            xmode = 1;
         }
         // String
         else if (sc.Match('\"')) {
            sc.SetState(SCE_HA_STRING);
            sc.Forward();
         }
         // Character
         else if (sc.Match('\'')) {
            sc.SetState(SCE_HA_CHARACTER);
            sc.Forward();
         }
         // Preprocessor
         else if (sc.atLineStart && sc.ch == '#') {
            mode = HA_MODE_DEFAULT;
            sc.SetState(SCE_HA_PREPROCESSOR);
            sc.Forward();
         }
         // Operator
         else if (IsAnOperatorChar(sc.ch)) {
            mode = HA_MODE_DEFAULT;
            sc.SetState(SCE_HA_OPERATOR);
         }
         // Braces and punctuation
         else if (sc.ch == ',' || sc.ch == ';'
               || sc.ch == '(' || sc.ch == ')'
               || sc.ch == '[' || sc.ch == ']'
               || sc.ch == '{' || sc.ch == '}') {
            sc.SetState(SCE_HA_OPERATOR);
            sc.Forward();
            sc.SetState(SCE_HA_DEFAULT);
         }
         // Keyword or Identifier
         else if (IsAWordStart(sc.ch)) {
            xmode = isupper(sc.ch) ? SCE_HA_CAPITAL : SCE_HA_IDENTIFIER;
            sc.SetState(SCE_HA_IDENTIFIER);
            sc.Forward();
         } else {
            if (sc.atLineEnd) {
                // Remember the line state for future incremental lexing
                styler.SetLineState(lineCurrent, (xmode << 4) | mode);
                lineCurrent++;
            }
            sc.Forward();
         }
      }
   }
   sc.Complete();
}

// External stuff - used for dynamic-loading, not implemented in wxStyledTextCtrl yet.
// Inspired by the caml external lexer - Credits to Robert Roessler - http://www.rftp.com
#ifdef BUILD_EXTERNAL_LEXER
static const char* LexerName = "haskell";

void EXT_LEXER_DECL Lex(unsigned int lexer, unsigned int startPos, int length, int initStyle,
                        char *words[], WindowID window, char *props)
{
   PropSetSimple ps;
   ps.SetMultiple(props);
   WindowAccessor wa(window, ps);

   int nWL = 0;
   for (; words[nWL]; nWL++) ;
   WordList** wl = new WordList* [nWL + 1];
   int i = 0;
   for (; i<nWL; i++)
   {
      wl[i] = new WordList();
      wl[i]->Set(words[i]);
   }
   wl[i] = 0;

   ColorizeHaskellDoc(startPos, length, initStyle, wl, wa);
   wa.Flush();
   for (i=nWL-1;i>=0;i--)
      delete wl[i];
   delete [] wl;
}

void EXT_LEXER_DECL Fold (unsigned int lexer, unsigned int startPos, int length, int initStyle,
                        char *words[], WindowID window, char *props)
{

}

int EXT_LEXER_DECL GetLexerCount()
{
   return 1;
}

void EXT_LEXER_DECL GetLexerName(unsigned int Index, char *name, int buflength)
{
   if (buflength > 0) {
      buflength--;
      int n = strlen(LexerName);
      if (n > buflength)
         n = buflength;
      memcpy(name, LexerName, n), name[n] = '\0';
   }
}
#endif

LexerModule lmHaskell(SCLEX_HASKELL, ColorizeHaskellDoc, "haskell");
