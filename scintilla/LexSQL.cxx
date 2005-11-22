// Scintilla source code edit control
/** @file LexSQL.cxx
 ** Lexer for SQL.
 **/
// Copyright 1998-2002 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>

#include "Platform.h"

#include "PropSet.h"
#include "Accessor.h"
#include "KeyWords.h"
#include "Scintilla.h"
#include "SciLexer.h"


static inline bool IsASpace(unsigned int ch) {
    return (ch == ' ') || ((ch >= 0x09) && (ch <= 0x0d));
}

static bool MatchIgnoreCaseSubstring(const char *s, Accessor &styler, unsigned int startPos) {
	char ch = styler.SafeGetCharAt(startPos);
	bool isSubword = false;
	if (tolower(ch) != *s)
		return false;
	s++;
	if (*s == '~')
	{
		isSubword = true;
		s++;
	}
	int n;
	for (n = 1; *s; n++) {
		if (*s == '~')
		{
			isSubword = true;
			s++;
		}
		if (isSubword && IsASpace(styler.SafeGetCharAt(startPos + n)))
			return true;
		if (*s != tolower((styler.SafeGetCharAt(startPos + n))))
			return false;
		s++;
	}
	return (IsASpace(styler.SafeGetCharAt(startPos + n)) 
		|| styler.SafeGetCharAt(startPos + n) == ';');
}

static void getCurrent(unsigned int start,
		unsigned int end,
		Accessor &styler,
		char *s,
		unsigned int len) {
	for (unsigned int i = 0; i < end - start + 1 && i < len; i++) {
		s[i] = static_cast<char>(tolower(styler[start + i]));
		s[i + 1] = '\0';
	}
}

static void classifyWordSQL(unsigned int start, unsigned int end, WordList *keywordlists[], Accessor &styler) {
	char s[100];
	bool wordIsNumber = isdigit(styler[start]) || (styler[start] == '.');
	for (unsigned int i = 0; i < end - start + 1 && i < 80; i++) {
		s[i] = static_cast<char>(tolower(styler[start + i]));
		s[i + 1] = '\0';
	}

	WordList &keywords1  = *keywordlists[0];
	WordList &keywords2  = *keywordlists[1];
//	WordList &kw_pldoc   = *keywordlists[2];
	WordList &kw_sqlplus = *keywordlists[3];
	WordList &kw_user1   = *keywordlists[4];
	WordList &kw_user2   = *keywordlists[5];
	WordList &kw_user3   = *keywordlists[6];
	WordList &kw_user4   = *keywordlists[7];

	char chAttr = SCE_SQL_IDENTIFIER;
	if (wordIsNumber)
		chAttr = SCE_SQL_NUMBER;
	else if (keywords1.InList(s))
			chAttr = SCE_SQL_WORD;
	else if (keywords2.InList(s)) 
			chAttr = SCE_SQL_WORD2;
	else if (kw_sqlplus.InListAbbreviated(s, '~'))
		chAttr = SCE_SQL_SQLPLUS;
	else if (kw_user1.InList(s))
		chAttr = SCE_SQL_USER1;
	else if (kw_user2.InList(s))
		chAttr = SCE_SQL_USER2;
	else if (kw_user3.InList(s))
		chAttr = SCE_SQL_USER3;
	else if (kw_user4.InList(s))
		chAttr = SCE_SQL_USER4;

	styler.ColourTo(end, chAttr);
}

static void ColouriseSQLDoc(unsigned int startPos, int length,
                            int initStyle, WordList *keywordlists[], Accessor &styler) {

	WordList &kw_pldoc = *keywordlists[2];
	styler.StartAt(startPos);

	bool fold = styler.GetPropertyInt("fold") != 0;
	bool sqlBackslashEscapes = styler.GetPropertyInt("sql.backslash.escapes", 0) != 0;
	int lineCurrent = styler.GetLine(startPos);
	int spaceFlags = 0;

	int state = initStyle;
	char chPrev = ' ';
	char chNext = styler[startPos];
	styler.StartSegment(startPos);
	unsigned int lengthDoc = startPos + length;
	for (unsigned int i = startPos; i < lengthDoc; i++) {
		char ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);

		if ((ch == '\r' && chNext != '\n') || (ch == '\n')) {
			int indentCurrent = styler.IndentAmount(lineCurrent, &spaceFlags);
			int lev = indentCurrent;
			if (!(indentCurrent & SC_FOLDLEVELWHITEFLAG)) {
				// Only non whitespace lines can be headers
				int indentNext = styler.IndentAmount(lineCurrent + 1, &spaceFlags);
				if (indentCurrent < (indentNext & ~SC_FOLDLEVELWHITEFLAG)) {
					lev |= SC_FOLDLEVELHEADERFLAG;
				}
			}
			if (fold) {
				styler.SetLevel(lineCurrent, lev);
			}
		}

		if (styler.IsLeadByte(ch)) {
			chNext = styler.SafeGetCharAt(i + 2);
			chPrev = ' ';
			i += 1;
			continue;
		}

		if (state == SCE_SQL_DEFAULT) {
			if (MatchIgnoreCaseSubstring("rem~ark", styler, i)) {
				styler.ColourTo(i - 1, state);
				state = SCE_SQL_SQLPLUS_COMMENT;
			} else if (MatchIgnoreCaseSubstring("pro~mpt", styler, i)) {
				styler.ColourTo(i - 1, state);
				state = SCE_SQL_SQLPLUS_PROMPT;
			} else if (iswordstart(ch)) {
				styler.ColourTo(i - 1, state);
				state = SCE_SQL_WORD;
			} else if (ch == '/' && chNext == '*') {
				styler.ColourTo(i - 1, state);
				if ((styler.SafeGetCharAt(i + 2)) == '*') {	// Support of Doxygen doc. style
					state = SCE_SQL_COMMENTDOC;
				} else {
					state = SCE_SQL_COMMENT;
				}
			} else if (ch == '-' && chNext == '-') {
				styler.ColourTo(i - 1, state);
				state = SCE_SQL_COMMENTLINE;
			} else if (ch == '#') {
				styler.ColourTo(i - 1, state);
				state = SCE_SQL_COMMENTLINEDOC;
			} else if (ch == '\'') {
				styler.ColourTo(i - 1, state);
				state = SCE_SQL_CHARACTER;
			} else if (ch == '"') {
				styler.ColourTo(i - 1, state);
				state = SCE_SQL_STRING;
			} else if (isoperator(ch)) {
				styler.ColourTo(i - 1, state);
				styler.ColourTo(i, SCE_SQL_OPERATOR);
			}
		} else if (state == SCE_SQL_WORD) {
			if (!iswordchar(ch)) {
				classifyWordSQL(styler.GetStartSegment(), i - 1, keywordlists, styler);
				state = SCE_SQL_DEFAULT;
				if (ch == '/' && chNext == '*') {
					if ((styler.SafeGetCharAt(i + 2)) == '*') {	// Support of Doxygen doc. style
						state = SCE_SQL_COMMENTDOC;
					} else {
						state = SCE_SQL_COMMENT;
					}
				} else if (ch == '-' && chNext == '-') {
					state = SCE_SQL_COMMENTLINE;
				} else if (ch == '#') {
					state = SCE_SQL_COMMENTLINEDOC;
				} else if (ch == '\'') {
					state = SCE_SQL_CHARACTER;
				} else if (ch == '"') {
					state = SCE_SQL_STRING;
				} else if (isoperator(ch)) {
					styler.ColourTo(i, SCE_SQL_OPERATOR);
				}
			}
		} else {
			if (state == SCE_SQL_COMMENT) {
				if (ch == '/' && chPrev == '*') {
					if (((i > (styler.GetStartSegment() + 2)) || ((initStyle == SCE_C_COMMENT) &&
					    (styler.GetStartSegment() == startPos)))) {
						styler.ColourTo(i, state);
						state = SCE_SQL_DEFAULT;
					}
				}
			} else if (state == SCE_SQL_COMMENTDOC) {
				if (ch == '/' && chPrev == '*') {
					if (((i > (styler.GetStartSegment() + 2)) || ((initStyle == SCE_SQL_COMMENTDOC) &&
					    (styler.GetStartSegment() == startPos)))) {
						styler.ColourTo(i, state);
						state = SCE_SQL_DEFAULT;
					}
				} else if (ch == '@') {
					// Verify that we have the conditions to mark a comment-doc-keyword
					if ((IsASpace(chPrev) || chPrev == '*') && (!IsASpace(chNext))) {
						styler.ColourTo(i - 1, state);
						state = SCE_SQL_COMMENTDOCKEYWORD;
					}
				}
			} else if (state == SCE_SQL_COMMENTLINE || state == SCE_SQL_COMMENTLINEDOC || state == SCE_SQL_SQLPLUS_COMMENT) {
				if (ch == '\r' || ch == '\n') {
					styler.ColourTo(i - 1, state);
					state = SCE_SQL_DEFAULT;
				}
			} else if (state == SCE_SQL_COMMENTDOCKEYWORD) {
				if (ch == '/' && chPrev == '*') {
					styler.ColourTo(i - 1, SCE_SQL_COMMENTDOCKEYWORDERROR);
					state = SCE_SQL_DEFAULT;
				} else if (!iswordchar(ch)) {
					char s[100];
					getCurrent(styler.GetStartSegment(), i - 1, styler, s, 30);
					if (!kw_pldoc.InList(s + 1)) {
						state = SCE_SQL_COMMENTDOCKEYWORDERROR;
					}
					styler.ColourTo(i - 1, state);
					state = SCE_SQL_COMMENTDOC;
				}
			} else if (state == SCE_SQL_SQLPLUS_PROMPT) {
				if (ch == '\r' || ch == '\n') {
					styler.ColourTo(i - 1, state);
					state = SCE_SQL_DEFAULT;
				}
			} else if (state == SCE_SQL_CHARACTER) {
				if (sqlBackslashEscapes && ch == '\\') {
					i++;
					ch = chNext;
					chNext = styler.SafeGetCharAt(i + 1);
				} else if (ch == '\'') {
					if (chNext == '\'') {
						i++;
					} else {
						styler.ColourTo(i, state);
						state = SCE_SQL_DEFAULT;
						i++;
					}
					ch = chNext;
					chNext = styler.SafeGetCharAt(i + 1);
				}
			} else if (state == SCE_SQL_STRING) {
				if (ch == '"') {
					if (chNext == '"') {
						i++;
					} else {
						styler.ColourTo(i, state);
						state = SCE_SQL_DEFAULT;
						i++;
					}
					ch = chNext;
					chNext = styler.SafeGetCharAt(i + 1);
				}
			}
			if (state == SCE_SQL_DEFAULT) {    // One of the above succeeded
				if (ch == '/' && chNext == '*') {
					if ((styler.SafeGetCharAt(i + 2)) == '*') {	// Support of Doxygen doc. style
						state = SCE_SQL_COMMENTDOC;
					} else {
						state = SCE_SQL_COMMENT;
					}
				} else if (ch == '-' && chNext == '-') {
					state = SCE_SQL_COMMENTLINE;
				} else if (ch == '#') {
					state = SCE_SQL_COMMENTLINEDOC;
				} else if (MatchIgnoreCaseSubstring("rem~ark", styler, i)) {
					state = SCE_SQL_SQLPLUS_COMMENT;
				} else if (MatchIgnoreCaseSubstring("pro~mpt", styler, i)) {
					state = SCE_SQL_SQLPLUS_PROMPT;
				} else if (ch == '\'') {
					state = SCE_SQL_CHARACTER;
				} else if (ch == '"') {
					state = SCE_SQL_STRING;
				} else if (iswordstart(ch)) {
					state = SCE_SQL_WORD;
				} else if (isoperator(ch)) {
					styler.ColourTo(i, SCE_SQL_OPERATOR);
				}
			}
		}
		chPrev = ch;
	}
	styler.ColourTo(lengthDoc - 1, state);
}

static bool IsStreamCommentStyle(int style) {
	return style == SCE_SQL_COMMENT ||
	       style == SCE_SQL_COMMENTDOC ||
	       style == SCE_SQL_COMMENTDOCKEYWORD ||
	       style == SCE_SQL_COMMENTDOCKEYWORDERROR;
}

// Store both the current line's fold level and the next lines in the
// level store to make it easy to pick up with each increment
// and to make it possible to fiddle the current level for "} else {".
static void FoldSQLDoc(unsigned int startPos, int length, int initStyle,
                       WordList *[], Accessor &styler) {
	bool foldComment = styler.GetPropertyInt("fold.comment") != 0;
	bool foldCompact = styler.GetPropertyInt("fold.compact", 1) != 0;
	unsigned int endPos = startPos + length;
	int visibleChars = 0;
	int lineCurrent = styler.GetLine(startPos);
	int levelCurrent = SC_FOLDLEVELBASE;
	if (lineCurrent > 0)
		levelCurrent = styler.LevelAt(lineCurrent-1) >> 16;
	int levelNext = levelCurrent;
	char chNext = styler[startPos];
	int styleNext = styler.StyleAt(startPos);
	int style = initStyle;
	bool endFound = false;
	for (unsigned int i = startPos; i < endPos; i++) {
		char ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);
		int stylePrev = style;
		style = styleNext;
		styleNext = styler.StyleAt(i + 1);
		bool atEOL = (ch == '\r' && chNext != '\n') || (ch == '\n');
		if (foldComment && IsStreamCommentStyle(style)) {
			if (!IsStreamCommentStyle(stylePrev)) {
				levelNext++;
			} else if (!IsStreamCommentStyle(styleNext) && !atEOL) {
				// Comments don't end at end of line and the next character may be unstyled.
				levelNext--;
			}
		}
		if (foldComment && (style == SCE_SQL_COMMENTLINE)) {
			if ((ch == '-') && (chNext == '-')) {
				char chNext2 = styler.SafeGetCharAt(i + 2);
				if (chNext2 == '{') {
					levelNext++;
				} else if (chNext2 == '}') {
					levelNext--;
				}
			}
		}
		if (style == SCE_SQL_WORD) {
			if (MatchIgnoreCaseSubstring("elsif", styler, i)) {
				// ignore elsif
				i += 4;
			} else if (MatchIgnoreCaseSubstring("if", styler, i)
				|| MatchIgnoreCaseSubstring("loop", styler, i)){
					if (endFound){
						// ignore
						endFound = false;
					} else {
						levelNext++;
					}
			} else if (MatchIgnoreCaseSubstring("begin", styler, i)){
				levelNext++;
			} else if (MatchIgnoreCaseSubstring("end", styler, i)) {
				endFound = true;
				levelNext--;
				if (levelNext < SC_FOLDLEVELBASE)
					levelNext = SC_FOLDLEVELBASE;
			}
		}
		if (atEOL) {
			int levelUse = levelCurrent;
			int lev = levelUse | levelNext << 16;
			if (visibleChars == 0 && foldCompact)
				lev |= SC_FOLDLEVELWHITEFLAG;
			if (levelUse < levelNext)
				lev |= SC_FOLDLEVELHEADERFLAG;
			if (lev != styler.LevelAt(lineCurrent)) {
				styler.SetLevel(lineCurrent, lev);
			}
			lineCurrent++;
			levelCurrent = levelNext;
			visibleChars = 0;
			endFound = false;
		}
		if (!isspacechar(ch))
			visibleChars++;
	}
}

static const char * const sqlWordListDesc[] = {
	"Keywords",
	"Database Objects",
	"PLDoc",
	"SQL*Plus",
	"User Keywords 1",
	"User Keywords 2",
	"User Keywords 3",
	"User Keywords 4",
	0
};

LexerModule lmSQL(SCLEX_SQL, ColouriseSQLDoc, "sql", FoldSQLDoc, sqlWordListDesc);
