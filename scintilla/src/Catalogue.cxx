// Scintilla source code edit control
/** @file KeyWords.cxx
 ** Colourise for particular languages.
 **/
// Copyright 1998-2002 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include <vector>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "LexerModule.h"
#include "Catalogue.h"

#ifdef SCI_NAMESPACE
using namespace Scintilla;
#endif

static std::vector<LexerModule *> lexerCatalogue;
static int nextLanguage = SCLEX_AUTOMATIC+1;

const LexerModule *Catalogue::Find(int language) {
	Scintilla_LinkLexers();
	for (std::vector<LexerModule *>::iterator it=lexerCatalogue.begin();
		it != lexerCatalogue.end(); ++it) {
		if ((*it)->GetLanguage() == language) {
			return *it;
		}
	}
	return 0;
}

const LexerModule *Catalogue::Find(const char *languageName) {
	Scintilla_LinkLexers();
	if (languageName) {
		for (std::vector<LexerModule *>::iterator it=lexerCatalogue.begin();
			it != lexerCatalogue.end(); ++it) {
			if ((*it)->languageName && (0 == strcmp((*it)->languageName, languageName))) {
				return *it;
			}
		}
	}
	return 0;
}

void Catalogue::AddLexerModule(LexerModule *plm) {
	if (plm->GetLanguage() == SCLEX_AUTOMATIC) {
		plm->language = nextLanguage;
		nextLanguage++;
	}
	lexerCatalogue.push_back(plm);
}

// Alternative historical name for Scintilla_LinkLexers
int wxForceScintillaLexers(void) {
	return Scintilla_LinkLexers();
}

// To add or remove a lexer, add or remove its file and run LexGen.py.

// Force a reference to all of the Scintilla lexers so that the linker will
// not remove the code of the lexers.
int Scintilla_LinkLexers() {

	static int initialised = 0;
	if (initialised)
		return 0;
	initialised = 1;

// Shorten the code that declares a lexer and ensures it is linked in by calling a method.
#define LINK_LEXER(lexer) extern LexerModule lexer; Catalogue::AddLexerModule(&lexer);

//++Autogenerated -- run src/LexGen.py to regenerate
//**\(\tLINK_LEXER(\*);\n\)
	LINK_LEXER(lmAda);
	LINK_LEXER(lmAsm);
	LINK_LEXER(lmBash);
	LINK_LEXER(lmCaml);
	LINK_LEXER(lmCmake);
	LINK_LEXER(lmCOBOL);
	LINK_LEXER(lmCoffeeScript);
	LINK_LEXER(lmCPP);
	LINK_LEXER(lmCss);
	LINK_LEXER(lmD);
	LINK_LEXER(lmDiff);
	LINK_LEXER(lmErlang);
	LINK_LEXER(lmF77);
	LINK_LEXER(lmForth);
	LINK_LEXER(lmFortran);
	LINK_LEXER(lmFreeBasic);
	LINK_LEXER(lmHaskell);
	LINK_LEXER(lmHTML);
	LINK_LEXER(lmLatex);
	LINK_LEXER(lmLISP);
	LINK_LEXER(lmLua);
	LINK_LEXER(lmMake);
	LINK_LEXER(lmMarkdown);
	// We use Octave instead of Matlab
	LINK_LEXER(lmNsis);
	LINK_LEXER(lmNull);
	LINK_LEXER(lmOctave);
	LINK_LEXER(lmPascal);
	LINK_LEXER(lmPerl);
	LINK_LEXER(lmPo);
	LINK_LEXER(lmProps);
	LINK_LEXER(lmPython);
	LINK_LEXER(lmR);
	LINK_LEXER(lmRuby);
	LINK_LEXER(lmSQL);
	LINK_LEXER(lmTCL);
	LINK_LEXER(lmTxt2tags);
	LINK_LEXER(lmVerilog);
	LINK_LEXER(lmVHDL);
	LINK_LEXER(lmXML);
	LINK_LEXER(lmYAML);

//--Autogenerated -- end of automatically generated section

	return 1;
}
