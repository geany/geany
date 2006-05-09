
SUBDIRS = include

noinst_LIBRARIES=libscintilla.a

CC=g++

AM_CXXFLAGS = -DGTK -DGTK2 -DSCI_LEXER -DG_THREADS_IMPL_NONE
#AM_CXXFLAGS = -DGTK -DGTK2 -DSCI_LEXER

LEXER_SRCS= \
LexAsm.cxx \
LexBash.cxx \
LexOMS.cxx \
LexCPP.cxx \
LexCaml.cxx \
LexConf.cxx \
LexCrontab.cxx \
LexCSS.cxx \
LexHTML.cxx \
LexOthers.cxx \
LexPascal.cxx \
LexPerl.cxx \
LexPython.cxx \
LexRuby.cxx \
LexSQL.cxx

SRCS= \
CallTip.cxx \
CellBuffer.cxx \
CharClassify.cxx \
ContractionState.cxx \
DocumentAccessor.cxx \
Document.cxx \
Editor.cxx \
Indicator.cxx \
KeyMap.cxx \
KeyWords.cxx \
LineMarker.cxx \
PropSet.cxx \
RESearch.cxx \
ScintillaBase.cxx \
StyleContext.cxx \
Style.cxx \
UniConversion.cxx \
ViewStyle.cxx \
WindowAccessor.cxx \
XPM.cxx \
AutoComplete.cxx \
ExternalLexer.cxx \
*.h \
PlatGTK.cxx \
ScintillaGTK.cxx \
$(LEXER_SRCS)

libscintilla_a_SOURCES = $(SRCS)

INCLUDES=-I$(top_srcdir) -Iinclude \
@PACKAGE_CFLAGS@

libscintilla_a_LIBADD = scintilla-marshal.o

scintilla-marshal.o: scintilla-marshal.c
	gcc @PACKAGE_CFLAGS@ -c scintilla-marshal.c

marshallers: scintilla-marshal.list
	glib-genmarshal --prefix scintilla_marshal scintilla-marshal.list --header > scintilla-marshal.h
	glib-genmarshal --prefix scintilla_marshal scintilla-marshal.list --body > scintilla-marshal.c

EXTRA_DIST=scintilla-marshal.list scintilla-marshal.c scintilla-marshal.h
