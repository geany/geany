
SUBDIRS = include

noinst_LIBRARIES=libscintilla.a

CC=g++

AM_CXXFLAGS = -DGTK -DGTK2 -DSCI_LEXER -DG_THREADS_IMPL_NONE

LEXER_SRCS= \
LexAsm.cxx \
LexBasic.cxx \
LexBash.cxx \
LexOMS.cxx \
LexCPP.cxx \
LexCaml.cxx \
LexConf.cxx \
LexCrontab.cxx \
LexCSS.cxx \
LexD.cxx \
LexFortran.cxx \
LexHTML.cxx \
LexLua.cxx \
LexOthers.cxx \
LexPascal.cxx \
LexPerl.cxx \
LexPython.cxx \
LexRuby.cxx \
LexSQL.cxx \
LexTCL.cxx \
LexVHDL.cxx \
LexHaskell.cxx

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
PositionCache.cxx \
PropSet.cxx \
RESearch.cxx \
RunStyles.cxx \
ScintillaBase.cxx \
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
PositionCache.h \
RESearch.h \
RunStyles.h \
ScintillaBase.h \
scintilla-marshal.h \
SplitVector.h \
StyleContext.h \
Style.h \
SVector.h \
UniConversion.h \
ViewStyle.h \
XPM.h \
$(LEXER_SRCS)

libscintilla_a_SOURCES = $(SRCS)

if MINGW
INCLUDES=-I$(top_srcdir) -I$(srcdir)/include -I/usr/local/cross-tools/include @GTK_CFLAGS@
else
INCLUDES=-I$(top_srcdir) -I$(srcdir)/include @GTK_CFLAGS@
endif

libscintilla_a_LIBADD = scintilla-marshal.o

scintilla-marshal.o: scintilla-marshal.c
	gcc @GTK_CFLAGS@ -c $(srcdir)/scintilla-marshal.c

marshallers: scintilla-marshal.list
	glib-genmarshal --prefix scintilla_marshal scintilla-marshal.list --header > scintilla-marshal.h
	glib-genmarshal --prefix scintilla_marshal scintilla-marshal.list --body > scintilla-marshal.c

EXTRA_DIST=scintilla-marshal.list scintilla-marshal.c scintilla-marshal.h License.txt
