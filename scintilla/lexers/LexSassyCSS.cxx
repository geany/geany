// Scintilla source code edit control
/** @file LexSassyCSS.cxx
 ** Lexer for Sassy CSS (SCSS)
 ** shamelessly copied from Lexer for Cascading Style Sheets
 **/
// Copyright 1998-2002 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

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

#ifdef SCI_NAMESPACE
using namespace Scintilla;
#endif


static inline bool IsAWordChar(const unsigned int ch) {
	/* FIXME:
	 * The CSS spec allows "ISO 10646 characters U+00A1 and higher" to be treated as word chars.
	 * Unfortunately, we are only getting string bytes here, and not full unicode characters. We cannot guarantee
	 * that our byte is between U+0080 - U+00A0 (to return false), so we have to allow all characters U+0080 and higher
	 */
	return ch >= 0x80 || isalnum(ch) || ch == '-' || ch == '_' || ch == '&';
}

inline bool IsScssOperator(const int ch) {
	if (!((ch < 0x80) && isalnum(ch)) &&
		(ch == '{' || ch == '}' || ch == ':' || ch == ',' || ch == ';' ||
		 ch == '.' || ch == '#' || ch == '!' || ch == '@' ||
		 /* CSS2 */
		 ch == '*' || ch == '>' || ch == '+' || ch == '=' || ch == '~' || ch == '|' ||
		 ch == '[' || ch == ']' || ch == '(' || ch == ')' ||
		 /* SCSS */
		 ch == '$' || ch == '&'
		)) {
		return true;
	}
	return false;
}

static void ColouriseScssDoc(unsigned int startPos, int length, int initStyle, WordList *keywordlists[], Accessor &styler) {
	WordList &css1Props = *keywordlists[0];
	WordList &pseudoClasses = *keywordlists[1];
	WordList &css2Props = *keywordlists[2];
	WordList &css3Props = *keywordlists[3];
	WordList &pseudoElements = *keywordlists[4];
	WordList &exProps = *keywordlists[5];
	WordList &exPseudoClasses = *keywordlists[6];
	WordList &exPseudoElements = *keywordlists[7];

	StyleContext sc(startPos, length, initStyle, styler);

	int lastState = -1; // before operator
	int lastStateC = -1; // before comment
	int lastStateS = -1; // before single-quoted/double-quoted string
	int op = ' '; // last operator
	int opPrev = ' '; // last operator

	for (; sc.More(); sc.Forward()) {
		if ((sc.state == SCE_SCSS_COMMENT && sc.Match('*', '/')) || (sc.state == SCE_SCSS_COMMENTLINE && sc.atLineEnd)) {
			if (lastStateC == -1) {
				// backtrack to get last state:
				// comments are like whitespace, so we must return to the previous state
				unsigned int i = startPos;
				for (; i > 0; i--) {
					if ((lastStateC = styler.StyleAt(i-1)) != SCE_SCSS_COMMENT) {
						if (lastStateC == SCE_SCSS_OPERATOR) {
							op = styler.SafeGetCharAt(i-1);
							opPrev = styler.SafeGetCharAt(i-2);
							while (--i) {
								lastState = styler.StyleAt(i-1);
								if (lastState != SCE_SCSS_OPERATOR && lastState != SCE_SCSS_COMMENT)
									break;
							}
							if (i == 0)
								lastState = SCE_SCSS_DEFAULT;
						}
						break;
					}
				}
				if (i == 0)
					lastStateC = SCE_SCSS_DEFAULT;
			}
			sc.Forward();
			sc.ForwardSetState(lastStateC);
		}

		if (sc.state == SCE_SCSS_COMMENT || sc.state == SCE_SCSS_COMMENTLINE)
			continue;

		if (sc.state == SCE_SCSS_DOUBLESTRING || sc.state == SCE_SCSS_SINGLESTRING) {
			if (sc.ch != (sc.state == SCE_SCSS_DOUBLESTRING ? '\"' : '\''))
				continue;
			unsigned int i = sc.currentPos;
			while (i && styler[i-1] == '\\')
				i--;
			if ((sc.currentPos - i) % 2 == 1)
				continue;
			sc.ForwardSetState(lastStateS);
		}

		if (sc.state == SCE_SCSS_OPERATOR) {
			if (op == ' ') {
				unsigned int i = startPos;
				op = styler.SafeGetCharAt(i-1);
				opPrev = styler.SafeGetCharAt(i-2);
				while (--i) {
					lastState = styler.StyleAt(i-1);
					if (lastState != SCE_SCSS_OPERATOR && lastState != SCE_SCSS_COMMENT)
						break;
				}
			}
			switch (op) {
			case '@':
				if (lastState == SCE_SCSS_DEFAULT)
					sc.SetState(SCE_SCSS_DIRECTIVE);
				break;
			case '>':
			case '+':
				if (lastState == SCE_SCSS_TAG || lastState == SCE_SCSS_CLASS || lastState == SCE_SCSS_ID ||
					lastState == SCE_SCSS_PSEUDOCLASS || lastState == SCE_SCSS_EXTENDED_PSEUDOCLASS || lastState == SCE_SCSS_UNKNOWN_PSEUDOCLASS)
					sc.SetState(SCE_SCSS_DEFAULT);
				break;
			case '[':
				if (lastState == SCE_SCSS_TAG || lastState == SCE_SCSS_DEFAULT || lastState == SCE_SCSS_CLASS || lastState == SCE_SCSS_ID ||
					lastState == SCE_SCSS_PSEUDOCLASS || lastState == SCE_SCSS_EXTENDED_PSEUDOCLASS || lastState == SCE_SCSS_UNKNOWN_PSEUDOCLASS)
					sc.SetState(SCE_SCSS_ATTRIBUTE);
				break;
			case ']':
				if (lastState == SCE_SCSS_ATTRIBUTE)
					sc.SetState(SCE_SCSS_TAG);
				break;
			case '{':
				if (lastState == SCE_SCSS_MEDIA)
					sc.SetState(SCE_SCSS_DEFAULT);
				else if (lastState == SCE_SCSS_TAG || lastState == SCE_SCSS_DIRECTIVE || lastState == SCE_SCSS_IDENTIFIER)
					sc.SetState(SCE_SCSS_IDENTIFIER);
				break;
			case '}':
				if (lastState == SCE_SCSS_DEFAULT || lastState == SCE_SCSS_VALUE || lastState == SCE_SCSS_IMPORTANT ||
					lastState == SCE_SCSS_IDENTIFIER || lastState == SCE_SCSS_IDENTIFIER2 || lastState == SCE_SCSS_IDENTIFIER3)
					sc.SetState(SCE_SCSS_DEFAULT);
				break;
			case '(':
				if (lastState == SCE_SCSS_PSEUDOCLASS)
					sc.SetState(SCE_SCSS_TAG);
				else if (lastState == SCE_SCSS_EXTENDED_PSEUDOCLASS)
					sc.SetState(SCE_SCSS_EXTENDED_PSEUDOCLASS);
				break;
			case ')':
				if (lastState == SCE_SCSS_TAG || lastState == SCE_SCSS_DEFAULT || lastState == SCE_SCSS_CLASS || lastState == SCE_SCSS_ID ||
					lastState == SCE_SCSS_PSEUDOCLASS || lastState == SCE_SCSS_EXTENDED_PSEUDOCLASS || lastState == SCE_SCSS_UNKNOWN_PSEUDOCLASS ||
					lastState == SCE_SCSS_PSEUDOELEMENT || lastState == SCE_SCSS_EXTENDED_PSEUDOELEMENT)
					sc.SetState(SCE_SCSS_TAG);
				break;
			case ':':
				if (lastState == SCE_SCSS_TAG || lastState == SCE_SCSS_DEFAULT || lastState == SCE_SCSS_CLASS || lastState == SCE_SCSS_ID ||
					lastState == SCE_SCSS_PSEUDOCLASS || lastState == SCE_SCSS_EXTENDED_PSEUDOCLASS || lastState == SCE_SCSS_UNKNOWN_PSEUDOCLASS ||
					lastState == SCE_SCSS_PSEUDOELEMENT || lastState == SCE_SCSS_EXTENDED_PSEUDOELEMENT)
					sc.SetState(SCE_SCSS_PSEUDOCLASS);
				else if (lastState == SCE_SCSS_IDENTIFIER || lastState == SCE_SCSS_IDENTIFIER2 ||
					lastState == SCE_SCSS_IDENTIFIER3 || lastState == SCE_SCSS_EXTENDED_IDENTIFIER ||
					lastState == SCE_SCSS_UNKNOWN_IDENTIFIER)
					sc.SetState(SCE_SCSS_VALUE);
				break;
			case '.':
				if (lastState == SCE_SCSS_TAG || lastState == SCE_SCSS_DEFAULT || lastState == SCE_SCSS_CLASS || lastState == SCE_SCSS_ID ||
					lastState == SCE_SCSS_PSEUDOCLASS || lastState == SCE_SCSS_EXTENDED_PSEUDOCLASS || lastState == SCE_SCSS_UNKNOWN_PSEUDOCLASS)
					sc.SetState(SCE_SCSS_CLASS);
				break;
			case '#':
				if (lastState == SCE_SCSS_TAG || lastState == SCE_SCSS_DEFAULT || lastState == SCE_SCSS_CLASS || lastState == SCE_SCSS_ID ||
					lastState == SCE_SCSS_PSEUDOCLASS || lastState == SCE_SCSS_EXTENDED_PSEUDOCLASS || lastState == SCE_SCSS_UNKNOWN_PSEUDOCLASS)
					sc.SetState(SCE_SCSS_ID);
				break;
			case ',':
			case '|':
			case '~':
				if (lastState == SCE_SCSS_TAG)
					sc.SetState(SCE_SCSS_DEFAULT);
				break;
			case ';':
				if (lastState == SCE_SCSS_DIRECTIVE)
					sc.SetState(SCE_SCSS_DEFAULT);
				else if (lastState == SCE_SCSS_VALUE || lastState == SCE_SCSS_IMPORTANT)
					sc.SetState(SCE_SCSS_IDENTIFIER);
				else if (lastState == SCE_SCSS_VARIABLE)
					sc.SetState(SCE_SCSS_DEFAULT);
				break;
			case '!':
				if (lastState == SCE_SCSS_VALUE)
					sc.SetState(SCE_SCSS_IMPORTANT);
				break;
			case '$':
				// SCSS variable name
				if (lastState == SCE_SCSS_VARIABLE || lastState == SCE_SCSS_IDENTIFIER || lastState == SCE_SCSS_TAG || lastState == SCE_SCSS_DEFAULT || lastState == SCE_SCSS_CLASS || lastState == SCE_SCSS_ID ||
					lastState == SCE_SCSS_PSEUDOCLASS || lastState == SCE_SCSS_EXTENDED_PSEUDOCLASS || lastState == SCE_SCSS_UNKNOWN_PSEUDOCLASS)
					sc.SetState(SCE_SCSS_VARIABLE);
				break;
			case '&':
				sc.SetState(SCE_SCSS_IDENTIFIER);
				break;
			}
		}

		if (IsAWordChar(sc.ch)) {
			if (sc.state == SCE_SCSS_DEFAULT)
				sc.SetState(SCE_SCSS_TAG);
			continue;
		}

		if (sc.ch == '*' && sc.state == SCE_SCSS_DEFAULT) {
			sc.SetState(SCE_SCSS_TAG);
			continue;
		}

		if (IsAWordChar(sc.chPrev) && (
			sc.state == SCE_SCSS_IDENTIFIER || sc.state == SCE_SCSS_IDENTIFIER2 ||
			sc.state == SCE_SCSS_IDENTIFIER3 || sc.state == SCE_SCSS_EXTENDED_IDENTIFIER ||
			sc.state == SCE_SCSS_UNKNOWN_IDENTIFIER ||
			sc.state == SCE_SCSS_PSEUDOCLASS || sc.state == SCE_SCSS_PSEUDOELEMENT ||
			sc.state == SCE_SCSS_EXTENDED_PSEUDOCLASS || sc.state == SCE_SCSS_EXTENDED_PSEUDOELEMENT ||
			sc.state == SCE_SCSS_UNKNOWN_PSEUDOCLASS ||
			sc.state == SCE_SCSS_IMPORTANT ||
			sc.state == SCE_SCSS_DIRECTIVE
		)) {
			char s[100];
			sc.GetCurrentLowered(s, sizeof(s));
			char *s2 = s;
			while (*s2 && !IsAWordChar(*s2))
				s2++;

			switch (sc.state) {
			case SCE_SCSS_VARIABLE:
				break;
			case SCE_SCSS_IDENTIFIER:
			case SCE_SCSS_IDENTIFIER2:
			case SCE_SCSS_IDENTIFIER3:
			case SCE_SCSS_EXTENDED_IDENTIFIER:
			case SCE_SCSS_UNKNOWN_IDENTIFIER:
				//~ if (s[0] == '&')	// SCSS nesting operator
					//~ sc.ChangeState(SCE_SCSS_EXTENDED_IDENTIFIER);
				//~ else if (css1Props.InList(s2))
				if (css1Props.InList(s2))
					sc.ChangeState(SCE_SCSS_IDENTIFIER);
				else if (css2Props.InList(s2))
					sc.ChangeState(SCE_SCSS_IDENTIFIER2);
				else if (css3Props.InList(s2))
					sc.ChangeState(SCE_SCSS_IDENTIFIER3);
				else if (exProps.InList(s2))
					sc.ChangeState(SCE_SCSS_EXTENDED_IDENTIFIER);
				else
					sc.ChangeState(SCE_SCSS_UNKNOWN_IDENTIFIER);
				break;
			case SCE_SCSS_PSEUDOCLASS:
			case SCE_SCSS_PSEUDOELEMENT:
			case SCE_SCSS_EXTENDED_PSEUDOCLASS:
			case SCE_SCSS_EXTENDED_PSEUDOELEMENT:
			case SCE_SCSS_UNKNOWN_PSEUDOCLASS:
				if (op == ':' && opPrev != ':' && pseudoClasses.InList(s2))
					sc.ChangeState(SCE_SCSS_PSEUDOCLASS);
				else if (opPrev == ':' && pseudoElements.InList(s2))
					sc.ChangeState(SCE_SCSS_PSEUDOELEMENT);
				else if ((op == ':' || (op == '(' && lastState == SCE_SCSS_EXTENDED_PSEUDOCLASS)) && opPrev != ':' && exPseudoClasses.InList(s2))
					sc.ChangeState(SCE_SCSS_EXTENDED_PSEUDOCLASS);
				else if (opPrev == ':' && exPseudoElements.InList(s2))
					sc.ChangeState(SCE_SCSS_EXTENDED_PSEUDOELEMENT);
				else
					sc.ChangeState(SCE_SCSS_UNKNOWN_PSEUDOCLASS);
				break;
			case SCE_SCSS_IMPORTANT:
				if (strcmp(s2, "important") != 0)
					sc.ChangeState(SCE_SCSS_VALUE);
				break;
			case SCE_SCSS_DIRECTIVE:
				if (op == '@' && strcmp(s2, "media") == 0)
					sc.ChangeState(SCE_SCSS_MEDIA);
				break;
			}
		}

		if (sc.ch != '.' && sc.ch != ':' && sc.ch != '#' && (
			sc.state == SCE_SCSS_CLASS || sc.state == SCE_SCSS_ID ||
			(sc.ch != '(' && sc.ch != ')' && ( /* This line of the condition makes it possible to extend pseudo-classes with parentheses */
				sc.state == SCE_SCSS_PSEUDOCLASS || sc.state == SCE_SCSS_PSEUDOELEMENT ||
				sc.state == SCE_SCSS_EXTENDED_PSEUDOCLASS || sc.state == SCE_SCSS_EXTENDED_PSEUDOELEMENT ||
				sc.state == SCE_SCSS_UNKNOWN_PSEUDOCLASS
			))
		))
			sc.SetState(SCE_SCSS_TAG);

		if (sc.Match('/', '*')) {
			lastStateC = sc.state;
			sc.SetState(SCE_SCSS_COMMENT);
			sc.Forward();
		} else if (sc.ch == '/' && sc.chNext == '/') {
			lastStateC = sc.state;
			sc.SetState(SCE_SCSS_COMMENTLINE);
			sc.Forward();
		} else if ((sc.state == SCE_SCSS_VALUE || sc.state == SCE_SCSS_ATTRIBUTE)
			&& (sc.ch == '\"' || sc.ch == '\'')) {
			lastStateS = sc.state;
			sc.SetState((sc.ch == '\"' ? SCE_SCSS_DOUBLESTRING : SCE_SCSS_SINGLESTRING));
		} else if (IsScssOperator(sc.ch)
			&& (sc.state != SCE_SCSS_ATTRIBUTE || sc.ch == ']')
			&& (sc.state != SCE_SCSS_VALUE || sc.ch == ';' || sc.ch == '}' || sc.ch == '!')
			&& ((sc.state != SCE_SCSS_DIRECTIVE && sc.state != SCE_SCSS_MEDIA) || sc.ch == ';' || sc.ch == '{')
		) {
			if (sc.state != SCE_SCSS_OPERATOR)
				lastState = sc.state;
			sc.SetState(SCE_SCSS_OPERATOR);
			op = sc.ch;
			opPrev = sc.chPrev;
		}
	}

	sc.Complete();
}

static void FoldSCSSDoc(unsigned int startPos, int length, int, WordList *[], Accessor &styler) {
	bool foldComment = styler.GetPropertyInt("fold.comment") != 0;
	bool foldCompact = styler.GetPropertyInt("fold.compact", 1) != 0;
	unsigned int endPos = startPos + length;
	int visibleChars = 0;
	int lineCurrent = styler.GetLine(startPos);
	int levelPrev = styler.LevelAt(lineCurrent) & SC_FOLDLEVELNUMBERMASK;
	int levelCurrent = levelPrev;
	char chNext = styler[startPos];
	bool inComment = (styler.StyleAt(startPos-1) == SCE_SCSS_COMMENT);
	for (unsigned int i = startPos; i < endPos; i++) {
		char ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);
		int style = styler.StyleAt(i);
		bool atEOL = (ch == '\r' && chNext != '\n') || (ch == '\n');
		if (foldComment) {
			if (!inComment && (style == SCE_SCSS_COMMENT))
				levelCurrent++;
			else if (inComment && (style != SCE_SCSS_COMMENT))
				levelCurrent--;
			inComment = (style == SCE_SCSS_COMMENT);
		}
		if (style == SCE_SCSS_OPERATOR) {
			if (ch == '{') {
				levelCurrent++;
			} else if (ch == '}') {
				levelCurrent--;
			}
		}
		if (atEOL) {
			int lev = levelPrev;
			if (visibleChars == 0 && foldCompact)
				lev |= SC_FOLDLEVELWHITEFLAG;
			if ((levelCurrent > levelPrev) && (visibleChars > 0))
				lev |= SC_FOLDLEVELHEADERFLAG;
			if (lev != styler.LevelAt(lineCurrent)) {
				styler.SetLevel(lineCurrent, lev);
			}
			lineCurrent++;
			levelPrev = levelCurrent;
			visibleChars = 0;
		}
		if (!isspacechar(ch))
			visibleChars++;
	}
	// Fill in the real level of the next line, keeping the current flags as they will be filled in later
	int flagsNext = styler.LevelAt(lineCurrent) & ~SC_FOLDLEVELNUMBERMASK;
	styler.SetLevel(lineCurrent, levelPrev | flagsNext);
}

static const char * const scssWordListDesc[] = {
	"CSS1 Properties",
	"Pseudo-classes",
	"CSS2 Properties",
	"CSS3 Properties",
	"Pseudo-elements",
	"Browser-Specific CSS Properties",
	"Browser-Specific Pseudo-classes",
	"Browser-Specific Pseudo-elements",
	0
};

LexerModule lmScss(SCLEX_SCSS, ColouriseScssDoc, "scss", FoldSCSSDoc, scssWordListDesc);
