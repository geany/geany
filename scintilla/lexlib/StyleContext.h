// Scintilla source code edit control
/** @file StyleContext.cxx
 ** Lexer infrastructure.
 **/
// Copyright 1998-2004 by Neil Hodgson <neilh@scintilla.org>
// This file is in the public domain.

#ifndef STYLECONTEXT_H
#define STYLECONTEXT_H

#ifdef SCI_NAMESPACE
namespace Scintilla {
#endif

static inline int MakeLowerCase(int ch) {
	if (ch < 'A' || ch > 'Z')
		return ch;
	else
		return ch - 'A' + 'a';
}

inline int UnicodeCodePoint(const unsigned char *us) {
	if (us[0] < 0xC2) {
		return us[0];
	} else if (us[0] < 0xE0) {
		return ((us[0] & 0x1F) << 6) + (us[1] & 0x3F);
	} else if (us[0] < 0xF0) {
		return ((us[0] & 0xF) << 12) + ((us[1] & 0x3F) << 6) + (us[2] & 0x3F);
	} else if (us[0] < 0xF5) {
		return ((us[0] & 0x7) << 18) + ((us[1] & 0x3F) << 12) + ((us[2] & 0x3F) << 6) + (us[3] & 0x3F);
	}
	return us[0];
}

inline int BytesInUnicodeCodePoint(int codePoint) {
	if (codePoint < 0x80)
		return 1;
	else if (codePoint < 0x800)
		return 2;
	else if (codePoint < 0x10000)
		return 3;
	else
		return 4;
}

// All languages handled so far can treat all characters >= 0x80 as one class
// which just continues the current token or starts an identifier if in default.
// DBCS treated specially as the second character can be < 0x80 and hence
// syntactically significant. UTF-8 avoids this as all trail bytes are >= 0x80
class StyleContext {
	LexAccessor &styler;
	unsigned int endPos;
	unsigned int lengthDocument;
	StyleContext &operator=(const StyleContext &);

	void GetNextChar(unsigned int pos) {
		chNext = static_cast<unsigned char>(styler.SafeGetCharAt(pos+1, 0));
		if (styler.Encoding() == encUnicode) {
			if (chNext >= 0x80) {
				unsigned char bytes[4] = { static_cast<unsigned char>(chNext), 0, 0, 0 };
				for (int trail=1; trail<3; trail++) {
					bytes[trail] = static_cast<unsigned char>(styler.SafeGetCharAt(pos+1+trail, 0));
					if (!((bytes[trail] >= 0x80) && (bytes[trail] < 0xc0))) {
						bytes[trail] = 0;
						break;
					}
				}
				chNext = UnicodeCodePoint(bytes);
			}
		} else if (styler.Encoding() == encDBCS) {
			if (styler.IsLeadByte(static_cast<char>(chNext))) {
				chNext = chNext << 8;
				chNext |= static_cast<unsigned char>(styler.SafeGetCharAt(pos+2, 0));
			}
		}
		// End of line?
		// Trigger on CR only (Mac style) or either on LF from CR+LF (Dos/Win)
		// or on LF alone (Unix). Avoid triggering two times on Dos/Win.
		if (currentLine < lineDocEnd)
			atLineEnd = static_cast<int>(pos) >= (lineStartNext-1);
		else // Last line
			atLineEnd = static_cast<int>(pos) >= lineStartNext;
	}

public:
	unsigned int currentPos;
	int currentLine;
	int lineDocEnd;
	int lineStartNext;
	bool atLineStart;
	bool atLineEnd;
	int state;
	int chPrev;
	int ch;
	int chNext;

	StyleContext(unsigned int startPos, unsigned int length,
                        int initStyle, LexAccessor &styler_, char chMask=31) :
		styler(styler_),
		endPos(startPos + length),
		currentPos(startPos),
		currentLine(-1),
		lineStartNext(-1),
		atLineEnd(false),
		state(initStyle & chMask), // Mask off all bits which aren't in the chMask.
		chPrev(0),
		ch(0),
		chNext(0) {
		styler.StartAt(startPos, chMask);
		styler.StartSegment(startPos);
		currentLine = styler.GetLine(startPos);
		lineStartNext = styler.LineStart(currentLine+1);
		lengthDocument = static_cast<unsigned int>(styler.Length());
		if (endPos == lengthDocument)
			endPos++;
		lineDocEnd = styler.GetLine(lengthDocument);
		atLineStart = static_cast<unsigned int>(styler.LineStart(currentLine)) == startPos;
		unsigned int pos = currentPos;
		ch = static_cast<unsigned char>(styler.SafeGetCharAt(pos, 0));
		if (styler.Encoding() == encUnicode) {
			// Get the current char
			GetNextChar(pos-1);
			ch = chNext;
			pos += BytesInUnicodeCodePoint(ch) - 1;
		} else if (styler.Encoding() == encDBCS) {
			if (styler.IsLeadByte(static_cast<char>(ch))) {
				pos++;
				ch = ch << 8;
				ch |= static_cast<unsigned char>(styler.SafeGetCharAt(pos, 0));
			}
		}
		GetNextChar(pos);
	}
	void Complete() {
		styler.ColourTo(currentPos - ((currentPos > lengthDocument) ? 2 : 1), state);
		styler.Flush();
	}
	bool More() const {
		return currentPos < endPos;
	}
	void Forward() {
		if (currentPos < endPos) {
			atLineStart = atLineEnd;
			if (atLineStart) {
				currentLine++;
				lineStartNext = styler.LineStart(currentLine+1);
			}
			chPrev = ch;
			if (styler.Encoding() == encUnicode) {
				currentPos += BytesInUnicodeCodePoint(ch);
			} else if (styler.Encoding() == encDBCS) {
				currentPos++;
				if (ch >= 0x100)
					currentPos++;
			} else {
				currentPos++;
			}
			ch = chNext;
			if (styler.Encoding() == encUnicode) {
				GetNextChar(currentPos + BytesInUnicodeCodePoint(ch)-1);
			} else if (styler.Encoding() == encDBCS) {
				GetNextChar(currentPos + ((ch >= 0x100) ? 1 : 0));
			} else {
				GetNextChar(currentPos);
			}
		} else {
			atLineStart = false;
			chPrev = ' ';
			ch = ' ';
			chNext = ' ';
			atLineEnd = true;
		}
	}
	void Forward(int nb) {
		for (int i = 0; i < nb; i++) {
			Forward();
		}
	}
	void ChangeState(int state_) {
		state = state_;
	}
	void SetState(int state_) {
		styler.ColourTo(currentPos - ((currentPos > lengthDocument) ? 2 : 1), state);
		state = state_;
	}
	void ForwardSetState(int state_) {
		Forward();
		styler.ColourTo(currentPos - ((currentPos > lengthDocument) ? 2 : 1), state);
		state = state_;
	}
	int LengthCurrent() {
		return currentPos - styler.GetStartSegment();
	}
	int GetRelative(int n) {
		return static_cast<unsigned char>(styler.SafeGetCharAt(currentPos+n, 0));
	}
	bool Match(char ch0) const {
		return ch == static_cast<unsigned char>(ch0);
	}
	bool Match(char ch0, char ch1) const {
		return (ch == static_cast<unsigned char>(ch0)) && (chNext == static_cast<unsigned char>(ch1));
	}
	bool Match(const char *s) {
		if (ch != static_cast<unsigned char>(*s))
			return false;
		s++;
		if (!*s)
			return true;
		if (chNext != static_cast<unsigned char>(*s))
			return false;
		s++;
		for (int n=2; *s; n++) {
			if (*s != styler.SafeGetCharAt(currentPos+n, 0))
				return false;
			s++;
		}
		return true;
	}
	bool MatchIgnoreCase(const char *s) {
		if (MakeLowerCase(ch) != static_cast<unsigned char>(*s))
			return false;
		s++;
		if (MakeLowerCase(chNext) != static_cast<unsigned char>(*s))
			return false;
		s++;
		for (int n=2; *s; n++) {
			if (static_cast<unsigned char>(*s) !=
				MakeLowerCase(static_cast<unsigned char>(styler.SafeGetCharAt(currentPos+n, 0))))
				return false;
			s++;
		}
		return true;
	}
	// Non-inline
	void GetCurrent(char *s, unsigned int len);
	void GetCurrentLowered(char *s, unsigned int len);
};

#ifdef SCI_NAMESPACE
}
#endif

#endif
