
SUBDIRS = include

noinst_LIBRARIES=libscintilla.a

AM_CXXFLAGS = -DNDEBUG -DGTK -DSCI_LEXER -DG_THREADS_IMPL_NONE

LEXER_SRCS= \
lexers/LexAda.cxx \
lexers/LexAsm.cxx \
lexers/LexBash.cxx \
lexers/LexBasic.cxx \
lexers/LexCOBOL.cxx \
lexers/LexCPP.cxx \
lexers/LexCSS.cxx \
lexers/LexCaml.cxx \
lexers/LexCmake.cxx \
lexers/LexD.cxx \
lexers/LexErlang.cxx \
lexers/LexForth.cxx \
lexers/LexFortran.cxx \
lexers/LexHTML.cxx \
lexers/LexHaskell.cxx \
lexers/LexLisp.cxx \
lexers/LexLua.cxx \
lexers/LexMarkdown.cxx \
lexers/LexMatlab.cxx \
lexers/LexNsis.cxx \
lexers/LexOthers.cxx \
lexers/LexPascal.cxx \
lexers/LexPerl.cxx \
lexers/LexPython.cxx \
lexers/LexR.cxx \
lexers/LexRuby.cxx \
lexers/LexSQL.cxx \
lexers/LexTCL.cxx \
lexers/LexTxt2tags.cxx \
lexers/LexVHDL.cxx \
lexers/LexVerilog.cxx \
lexers/LexYAML.cxx

SRCS= \
gtk/Converter.h \
gtk/PlatGTK.cxx \
gtk/ScintillaGTK.cxx \
gtk/scintilla-marshal.c \
gtk/scintilla-marshal.h \
lexlib/Accessor.cxx \
lexlib/Accessor.h \
lexlib/CharacterSet.cxx \
lexlib/CharacterSet.h \
lexlib/LexAccessor.h \
lexlib/LexerBase.cxx \
lexlib/LexerBase.h \
lexlib/LexerModule.cxx \
lexlib/LexerModule.h \
lexlib/LexerNoExceptions.cxx \
lexlib/LexerNoExceptions.h \
lexlib/LexerSimple.cxx \
lexlib/LexerSimple.h \
lexlib/OptionSet.h \
lexlib/PropSetSimple.cxx \
lexlib/PropSetSimple.h \
lexlib/SparseState.h \
lexlib/StyleContext.cxx \
lexlib/StyleContext.h \
lexlib/WordList.cxx \
lexlib/WordList.h \
src/AutoComplete.cxx \
src/AutoComplete.h \
src/CallTip.cxx \
src/CallTip.h \
src/Catalogue.cxx \
src/Catalogue.h \
src/CellBuffer.cxx \
src/CellBuffer.h \
src/CharClassify.cxx \
src/CharClassify.h \
src/ContractionState.cxx \
src/ContractionState.h \
src/Decoration.cxx \
src/Decoration.h \
src/Document.cxx \
src/Document.h \
src/Editor.cxx \
src/Editor.h \
src/ExternalLexer.cxx \
src/ExternalLexer.h \
src/FontQuality.h \
src/Indicator.cxx \
src/Indicator.h \
src/KeyMap.cxx \
src/KeyMap.h \
src/LineMarker.cxx \
src/LineMarker.h \
src/Partitioning.h \
src/PerLine.cxx \
src/PerLine.h \
src/PositionCache.cxx \
src/PositionCache.h \
src/RESearch.cxx \
src/RESearch.h \
src/RunStyles.cxx \
src/RunStyles.h \
src/SVector.h \
src/ScintillaBase.cxx \
src/ScintillaBase.h \
src/Selection.cxx \
src/Selection.h \
src/SplitVector.h \
src/Style.cxx \
src/Style.h \
src/UniConversion.cxx \
src/UniConversion.h \
src/ViewStyle.cxx \
src/ViewStyle.h \
src/XPM.cxx \
src/XPM.h \
$(LEXER_SRCS)

libscintilla_a_SOURCES = $(SRCS)

INCLUDES=-I$(top_srcdir) -I$(srcdir)/include -I$(srcdir)/src -I$(srcdir)/lexlib @GTK_CFLAGS@

marshallers: gtk/scintilla-marshal.list
	glib-genmarshal --prefix scintilla_marshal gtk/scintilla-marshal.list --header > gtk/scintilla-marshal.h
	glib-genmarshal --prefix scintilla_marshal gtk/scintilla-marshal.list --body > gtk/scintilla-marshal.c

EXTRA_DIST=gtk/scintilla-marshal.list License.txt README version.txt \
	makefile.win32
