
SUBDIRS = include

noinst_LIBRARIES=libscintilla.a

AM_CXXFLAGS = -DNDEBUG -DGTK -DGTK2 -DSCI_LEXER -DG_THREADS_IMPL_NONE

LEXER_SRCS= \
LexAda.cxx \
LexAsm.cxx \
LexBasic.cxx \
LexBash.cxx \
LexCPP.cxx \
LexCaml.cxx \
LexCmake.cxx \
LexCSS.cxx \
LexD.cxx \
LexForth.cxx \
LexFortran.cxx \
LexHaskell.cxx \
LexHTML.cxx \
LexLisp.cxx \
LexLua.cxx \
LexMarkdown.cxx \
LexMatlab.cxx \
LexNsis.cxx \
LexOthers.cxx \
LexPascal.cxx \
LexPerl.cxx \
LexPython.cxx \
LexR.cxx \
LexRuby.cxx \
LexSQL.cxx \
LexTxt2tags.cxx \
LexTCL.cxx \
LexVHDL.cxx \
LexVerilog.cxx \
LexYAML.cxx

SRCS= \
CallTip.cxx \
CellBuffer.cxx \
CharClassify.cxx \
ContractionState.cxx \
Decoration.cxx \
DocumentAccessor.cxx \
Document.cxx \
Editor.cxx \
Indicator.cxx \
KeyMap.cxx \
KeyWords.cxx \
LineMarker.cxx \
PerLine.cxx \
PositionCache.cxx \
PropSet.cxx \
RESearch.cxx \
RunStyles.cxx \
ScintillaBase.cxx \
Selection.cxx \
StyleContext.cxx \
Style.cxx \
UniConversion.cxx \
ViewStyle.cxx \
WindowAccessor.cxx \
XPM.cxx \
AutoComplete.cxx \
ExternalLexer.cxx \
PlatGTK.cxx \
ScintillaGTK.cxx \
AutoComplete.h \
CallTip.h \
CellBuffer.h \
CharClassify.h \
CharacterSet.h \
ContractionState.h \
Converter.h \
Decoration.h \
DocumentAccessor.h \
Document.h \
Editor.h \
ExternalLexer.h \
Indicator.h \
KeyMap.h \
LineMarker.h \
Partitioning.h \
PerLine.h \
PositionCache.h \
PropSetSimple.h \
RESearch.h \
RunStyles.h \
ScintillaBase.h \
scintilla-marshal.h \
Selection.h \
SplitVector.h \
StyleContext.h \
Style.h \
SVector.h \
UniConversion.h \
ViewStyle.h \
XPM.h \
scintilla-marshal.c \
$(LEXER_SRCS)

libscintilla_a_SOURCES = $(SRCS)

INCLUDES=-I$(top_srcdir) -I$(srcdir)/include @GTK_CFLAGS@

marshallers: scintilla-marshal.list
	glib-genmarshal --prefix scintilla_marshal scintilla-marshal.list --header > scintilla-marshal.h
	glib-genmarshal --prefix scintilla_marshal scintilla-marshal.list --body > scintilla-marshal.c

EXTRA_DIST=scintilla-marshal.list scintilla-marshal.c scintilla-marshal.h License.txt
