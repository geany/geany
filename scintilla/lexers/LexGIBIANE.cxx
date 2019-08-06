/* Scintilla source code edit control
 * @file LexGIBIANE.cxx
 * Lexer for GIBIANE.
 *
 * Copyright 2019 Thibault Lindecker (Communication & Systemes) - thibault.lindecker@c-s.fr
 * The License.txt file describes the conditions under which this software may be distributed. */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"

using namespace Scintilla;

class Is_ {
	private:
		static bool ret;
		bool parenthesis(char c);
		bool equal(char c);
		bool digit(char c);
	public:
		Is_();
		bool lineend(char c);
		bool blanck(char c);
		bool ANumber(char c, char cPrev, char cNext);
		bool SPECIAL(char c);
		bool quote(char c);
		bool semicolon(char c);
};
Is_::Is_(void){}
bool Is_::ret = false;
bool Is_::blanck(char c) {
	return ((c == ' ') || (c == 0x09) || (c == 0x0b));
}
bool Is_::lineend(char c) {
	return ((c == '\n') || (c == '\r'));
}
bool Is_::semicolon(char c) {
	return (c == ';');
}
bool Is_::parenthesis(char c) {
	return c == '(' || c == ')';
}
bool Is_::equal(char c) {
	return c == '=';
}
bool Is_::digit(char c) {
	return IsADigit(c);
}
bool Is_::quote(char c) {
	return (c == '\'');
}
bool Is_::SPECIAL(char c){
	return (blanck(c) || lineend(c) || semicolon(c) || parenthesis(c) || equal(c));
}
bool Is_::ANumber(char c, char cPrev, char cNext) {
	if (digit(c) || (c == '.' && digit(cNext))) {
		if(cPrev == '.' || (digit(cPrev) && ret) || SPECIAL(cPrev)){
			if(cNext == '.' || digit(cNext) || SPECIAL(cNext)){
				ret = true;
				return ret;
			}
		}
	}
	ret = false;
	return ret;
}

/* Set colorization for GIBIANE language. See the file 'data/filedefs/filetypes.gibiane' */
static void ColouriseGIBIANEDoc(Sci_PositionU startPos, Sci_Position length, int initStyle,
	WordList *keywordlists[], Accessor &styler) {

	Is_ Is;
	WordList &keywords  = *keywordlists[0];
	WordList &keywords2 = *keywordlists[1];
	WordList &keywords3 = *keywordlists[2];
	WordList &keywords4 = *keywordlists[3];
	WordList &keywords5 = *keywordlists[4];
	WordList &keywords6 = *keywordlists[5];
	WordList &keywords7 = *keywordlists[6];
	WordList &keywords8 = *keywordlists[7];
	WordList &keywords9 = *keywordlists[8];
	styler.StartAt(startPos);

	Sci_Position endPos = startPos + length;
	startPos = styler.LineStart(styler.GetLine(startPos));
	initStyle = styler.StyleAt(startPos - 1);
	length = endPos - startPos;

	StyleContext sc(startPos, length, initStyle, styler);

	Sci_Position posLineStart = 0;
	int numNonBlank = 0, prevState = 0;

	for (; sc.More(); sc.Forward()) {

		char s[100];
		sc.GetCurrentLowered(s, sizeof(s));
		char ch = s[0];
		char *s2 = &s[1];

		if (sc.state == SCE_GIBIANE_COMMENT) {
			if (sc.atLineEnd) {
				sc.SetState(SCE_GIBIANE_DEFAULT);
				sc.ChangeState(SCE_GIBIANE_DEFAULT);
			}
		} else if (sc.state == SCE_GIBIANE_STRING) {
			if (sc.ch == '\'') {
			 /* In GIBIANE, we can protect operator with apostrophe.
			  * So we have to check for operator now. */
				if ( keywords.InList(s2) ) {
						sc.Forward();
						sc.ChangeState(SCE_GIBIANE_WORD);
						sc.SetState(SCE_GIBIANE_DEFAULT);
				}
				else if ( keywords2.InList(s2) ) {
						sc.Forward();
						sc.ChangeState(SCE_GIBIANE_WORD2);
						sc.SetState(SCE_GIBIANE_DEFAULT);
				} else if ( keywords3.InList(s2) ) {
						sc.Forward();
						sc.ChangeState(SCE_GIBIANE_WORD3);
						sc.SetState(SCE_GIBIANE_DEFAULT);
				} else if ( keywords4.InList(s2) ) {
						sc.Forward();
						sc.ChangeState(SCE_GIBIANE_WORD4);
						sc.SetState(SCE_GIBIANE_DEFAULT);
				} else if ( keywords5.InList(s2) ) {
						sc.Forward();
						sc.ChangeState(SCE_GIBIANE_WORD5);
						sc.SetState(SCE_GIBIANE_DEFAULT);
				} else if ( keywords6.InList(s2) ) {
						sc.Forward();
						sc.ChangeState(SCE_GIBIANE_WORD6);
						sc.SetState(SCE_GIBIANE_DEFAULT);
				} else if ( keywords7.InList(s2) ) {
						sc.Forward();
						sc.ChangeState(SCE_GIBIANE_WORD7);
						sc.SetState(SCE_GIBIANE_DEFAULT);
				} else if ( keywords8.InList(s2) ) {
						sc.Forward();
						sc.ChangeState(SCE_GIBIANE_WORD8);
						sc.SetState(SCE_GIBIANE_DEFAULT);
				}
				else{
						sc.ForwardSetState(SCE_GIBIANE_DEFAULT);
				}
			}
		}
		else if (sc.state == SCE_GIBIANE_NUMBER) {
			sc.SetState(SCE_GIBIANE_DEFAULT);
		}
		else if (Is.SPECIAL(ch)) {
			sc.SetState(SCE_GIBIANE_DEFAULT);
		}

		if (sc.state == SCE_GIBIANE_DEFAULT) {

			/* In GIBIANE, comments line start with '*'. */
			if (sc.ch == '*' && (Is.lineend(sc.chPrev) || sc.currentPos == startPos)) {
				sc.SetState(SCE_GIBIANE_COMMENT);
			} else if (sc.ch == '\'') {
				sc.SetState(SCE_GIBIANE_STRING);
			}
			else if (Is.ANumber(sc.ch, sc.chPrev, sc.chNext)) {
				sc.SetState(SCE_GIBIANE_NUMBER);
			}
			 /* We check for operator */
			else if (Is.SPECIAL(sc.ch)) {
				if ( keywords.InList(s) ) {
					sc.ChangeState(SCE_GIBIANE_WORD);
					sc.SetState(SCE_GIBIANE_DEFAULT);
				}
				else if ( keywords2.InList(s) ) {
					sc.ChangeState(SCE_GIBIANE_WORD2);
					sc.SetState(SCE_GIBIANE_DEFAULT);
				} else if ( keywords3.InList(s) ) {
					sc.ChangeState(SCE_GIBIANE_WORD3);
					sc.SetState(SCE_GIBIANE_DEFAULT);
				} else if ( keywords4.InList(s) ) {
					sc.ChangeState(SCE_GIBIANE_WORD4);
					sc.SetState(SCE_GIBIANE_DEFAULT);
				} else if ( keywords5.InList(s) ) {
					sc.ChangeState(SCE_GIBIANE_WORD5);
					sc.SetState(SCE_GIBIANE_DEFAULT);
				} else if ( keywords6.InList(s) ) {
					sc.ChangeState(SCE_GIBIANE_WORD6);
					sc.SetState(SCE_GIBIANE_DEFAULT);
				} else if ( keywords7.InList(s) ) {
					sc.ChangeState(SCE_GIBIANE_WORD7);
					sc.SetState(SCE_GIBIANE_DEFAULT);
				} else if ( keywords8.InList(s) ) {
					sc.ChangeState(SCE_GIBIANE_WORD8);
					sc.SetState(SCE_GIBIANE_DEFAULT);
				} else {
					sc.ChangeState(SCE_GIBIANE_DEFAULT);
					sc.SetState(SCE_GIBIANE_DEFAULT);
				}
			}
			else if (sc.ch == '.'){
				sc.ChangeState(SCE_GIBIANE_TABLE);
				sc.SetState(SCE_GIBIANE_DEFAULT);
			}
		}

	}
	sc.Complete();
}

/* Set level.
 * 1) we manage the 'if - else' block :
 *  SI;
 *    do something
 *  SINON;
 *    do something
 *  FINSI;
 * 2) we manage the 'for loop' block :
 *  REPETER I 10;
 *    do something
 *  FIN I;
 * 3) we manage the 'DEBM' block :
 *  DEBMETH A;
 *    do something
 *  FINMETH B;
 * 4) we manage the 'DEBP' block :
 *  DEBPROC myprocedure toto*'TABL';
 *    do something
 *  FINPROC titi*'MOTS' tata*'ENTIER'
 * */
static void FoldGIBIANEDoc(Sci_PositionU startPos, Sci_Position length, int initStyle,
	WordList *[], Accessor &styler) {

	Is_ Is;
	Sci_PositionU endPos = startPos + length;
	int levelDeltaNext = 0;
	Sci_Position lineCurrent = styler.GetLine(startPos);
	static bool atEOL = false;
	static bool atEOLPast = atEOL;
	bool isPrevLine;

	if (lineCurrent > 0) {
		lineCurrent--;
		startPos = styler.LineStart(lineCurrent);
		isPrevLine = true;
	} else {
		isPrevLine = false;
	}
	char chNext = styler[startPos];
	int styleNext = styler.StyleAt(startPos);
	int style = initStyle;
	Sci_PositionU lastStart = 0;
	int visibleChars = 0;
	int levelCurrent = styler.LevelAt(lineCurrent) & SC_FOLDLEVELNUMBERMASK;
	static bool line_comment = false;

	for (Sci_PositionU i = startPos; i < endPos; i++) {

		char ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);
		int stylePrev = style;
		style = styleNext;
		styleNext = styler.StyleAt(i + 1);

		if(stylePrev == SCE_GIBIANE_DEFAULT){
			lastStart = i;
		}
		if( Is.blanck(ch) || Is.lineend(ch) || Is.quote(ch) ){
			lastStart = i+1;
		}

		if( (ch == '*' && (atEOLPast || atEOL)) || line_comment ){
			line_comment = true;
			lastStart = i+1;
			visibleChars = 0;
		}

		if( (Is.SPECIAL(chNext) || Is.quote(chNext)) && !line_comment && style != SCE_GIBIANE_STRING ){

			char s[32];
			Sci_PositionU k;
			for(k=0; (k<31 ) && (k<i-lastStart+1 ); k++) {
				s[k] = static_cast<char>(tolower(styler[lastStart+k]));
			}
			s[k] = '\0';
			int keyword_min_size = ((int)k < 4) ? 4 : (int)k;

			if (strcmp(s, "si") == 0 || strncmp(s, "repeter", keyword_min_size) == 0 ||
				strncmp(s, "debproc", keyword_min_size) == 0 || strncmp(s, "debmeth", keyword_min_size) == 0){

				levelDeltaNext++;

			}
			else if(strncmp(s, "finsi", keyword_min_size) == 0 || strcmp(s, "fin") == 0 ||
					strncmp(s, "finproc", keyword_min_size) == 0 || strncmp(s, "finmeth", keyword_min_size) == 0){

				if(strcmp(s, "fin") == 0){ /* On doit alors verifier que c'est pas un "fin;" mais bien un "fin boucle;"*/
					char c = 0;
					Sci_PositionU p;
					for(p=lastStart+k; p<endPos; p++){
						c = static_cast<char>(tolower(styler[p]));
						if(! Is.blanck(c) && ! Is.semicolon(c) && ! Is.quote(c)){ //On n'est pas sur un "fin ;" mais bien un "fin boucle;"
							levelDeltaNext--;
							break;
						}
						else if(Is.semicolon(c)){
							break;
						}
					}
				}
				else{
					levelDeltaNext--;
				}

			}
			else if(strncmp(s, "sinon", keyword_min_size) == 0){

				if(!isPrevLine){
					levelCurrent--;
				}
				levelDeltaNext++;

			}

		}

		atEOLPast = atEOL;
		atEOL = (ch == '\r' && chNext != '\n') || (ch == '\n');
		if (atEOL) {
			int lev = levelCurrent;
			bool foldCompact = styler.GetPropertyInt("fold.compact", 1) != 0;

			if (visibleChars == 0 && foldCompact)
				lev |= SC_FOLDLEVELWHITEFLAG;
			if ((levelDeltaNext > 0) && (visibleChars > 0))
				lev |= SC_FOLDLEVELHEADERFLAG;
			if (lev != styler.LevelAt(lineCurrent))
				styler.SetLevel(lineCurrent, lev);

			lineCurrent++;
			levelCurrent += levelDeltaNext;
			levelDeltaNext = 0;
			visibleChars = 0;
			isPrevLine = false;
			line_comment = false;
		}

		if (!isspacechar(ch)) visibleChars++;
	}

}

static const char *const GIBIANEWordLists[] = {
	"conditional",
	"test1",
	"loop",
	"task",
	"element",
	"function1",
	"function2",
	"operator",
	0
};

LexerModule lmGIBIANE(SCLEX_GIBIANE, ColouriseGIBIANEDoc, "GIBIANE", FoldGIBIANEDoc, GIBIANEWordLists);
