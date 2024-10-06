noinst_LTLIBRARIES = libscintilla.la liblexilla.la

AM_CXXFLAGS = -DNDEBUG -DGTK -DSCI_LEXER -DNO_CXX11_REGEX -std=c++17
AM_CPPFLAGS = @GTK_CFLAGS@ @LIBGEANY_CFLAGS@

scintilla_includedir = $(includedir)/geany/scintilla/
scintilla_include_HEADERS =            \
include/Scintilla.h                    \
include/Scintilla.iface                \
include/ScintillaWidget.h              \
include/Sci_Position.h                 \
lexilla/include/SciLexer.h

LEXER_SRCS =                           \
lexilla/lexers/LexAbaqus.cxx           \
lexilla/lexers/LexAda.cxx              \
lexilla/lexers/LexAsciidoc.cxx         \
lexilla/lexers/LexAsm.cxx              \
lexilla/lexers/LexAU3.cxx              \
lexilla/lexers/LexBash.cxx             \
lexilla/lexers/LexBasic.cxx            \
lexilla/lexers/LexBatch.cxx            \
lexilla/lexers/LexCIL.cxx              \
lexilla/lexers/LexCOBOL.cxx            \
lexilla/lexers/LexCPP.cxx              \
lexilla/lexers/LexCSS.cxx              \
lexilla/lexers/LexCaml.cxx             \
lexilla/lexers/LexCmake.cxx            \
lexilla/lexers/LexCoffeeScript.cxx     \
lexilla/lexers/LexD.cxx                \
lexilla/lexers/LexDiff.cxx             \
lexilla/lexers/LexErlang.cxx           \
lexilla/lexers/LexForth.cxx            \
lexilla/lexers/LexFortran.cxx          \
lexilla/lexers/LexGDScript.cxx         \
lexilla/lexers/LexHTML.cxx             \
lexilla/lexers/LexHaskell.cxx          \
lexilla/lexers/LexJulia.cxx            \
lexilla/lexers/LexLaTeX.cxx            \
lexilla/lexers/LexLisp.cxx             \
lexilla/lexers/LexLua.cxx              \
lexilla/lexers/LexMake.cxx             \
lexilla/lexers/LexMarkdown.cxx         \
lexilla/lexers/LexMatlab.cxx           \
lexilla/lexers/LexNim.cxx              \
lexilla/lexers/LexNsis.cxx             \
lexilla/lexers/LexNull.cxx             \
lexilla/lexers/LexPascal.cxx           \
lexilla/lexers/LexPerl.cxx             \
lexilla/lexers/LexPowerShell.cxx       \
lexilla/lexers/LexProps.cxx            \
lexilla/lexers/LexPython.cxx           \
lexilla/lexers/LexPO.cxx               \
lexilla/lexers/LexR.cxx                \
lexilla/lexers/LexRaku.cxx             \
lexilla/lexers/LexRuby.cxx             \
lexilla/lexers/LexRust.cxx             \
lexilla/lexers/LexSmalltalk.cxx        \
lexilla/lexers/LexSQL.cxx              \
lexilla/lexers/LexTCL.cxx              \
lexilla/lexers/LexTxt2tags.cxx         \
lexilla/lexers/LexVHDL.cxx             \
lexilla/lexers/LexVerilog.cxx          \
lexilla/lexers/LexVisualProlog.cxx     \
lexilla/lexers/LexYAML.cxx             \
lexilla/lexers/LexZig.cxx

LEXLIB_SRCS =                          \
lexilla/include/Lexilla.h              \
lexilla/include/SciLexer.h             \
lexilla/lexlib/Accessor.cxx            \
lexilla/lexlib/Accessor.h              \
lexilla/lexlib/CatalogueModules.h      \
lexilla/lexlib/CharacterCategory.cxx   \
lexilla/lexlib/CharacterCategory.h     \
lexilla/lexlib/CharacterSet.cxx        \
lexilla/lexlib/CharacterSet.h          \
lexilla/lexlib/DefaultLexer.cxx        \
lexilla/lexlib/DefaultLexer.h          \
lexilla/lexlib/LexAccessor.cxx         \
lexilla/lexlib/LexAccessor.h           \
lexilla/lexlib/LexerBase.cxx           \
lexilla/lexlib/LexerBase.h             \
lexilla/lexlib/LexerModule.cxx         \
lexilla/lexlib/LexerModule.h           \
lexilla/lexlib/LexerNoExceptions.cxx   \
lexilla/lexlib/LexerNoExceptions.h     \
lexilla/lexlib/LexerSimple.cxx         \
lexilla/lexlib/LexerSimple.h           \
lexilla/lexlib/OptionSet.h             \
lexilla/lexlib/PropSetSimple.cxx       \
lexilla/lexlib/PropSetSimple.h         \
lexilla/lexlib/SparseState.h           \
lexilla/lexlib/StringCopy.h            \
lexilla/lexlib/StyleContext.cxx        \
lexilla/lexlib/StyleContext.h          \
lexilla/lexlib/SubStyles.h             \
lexilla/lexlib/WordList.cxx            \
lexilla/lexlib/WordList.h              \
lexilla/src/Lexilla.cxx

SRCS =                                 \
include/ILexer.h                       \
include/ILoader.h                      \
include/ScintillaCall.h                \
include/ScintillaMessages.h            \
include/ScintillaStructures.h          \
include/ScintillaTypes.h               \
include/ScintillaWidget.h              \
gtk/Converter.h                        \
gtk/PlatGTK.cxx                        \
gtk/ScintillaGTK.cxx                   \
gtk/ScintillaGTK.h                     \
gtk/ScintillaGTKAccessible.cxx         \
gtk/ScintillaGTKAccessible.h           \
gtk/scintilla-marshal.c                \
gtk/scintilla-marshal.h                \
gtk/Wrappers.h                         \
src/AutoComplete.cxx                   \
src/AutoComplete.h                     \
src/CallTip.cxx                        \
src/CallTip.h                          \
src/CaseConvert.cxx                    \
src/CaseConvert.h                      \
src/CaseFolder.cxx                     \
src/CaseFolder.h                       \
src/CellBuffer.cxx                     \
src/CellBuffer.h                       \
src/ChangeHistory.cxx                  \
src/ChangeHistory.h                    \
src/CharacterCategoryMap.cxx           \
src/CharacterCategoryMap.h             \
src/CharacterType.cxx                  \
src/CharacterType.h                    \
src/CharClassify.cxx                   \
src/CharClassify.h                     \
src/ContractionState.cxx               \
src/ContractionState.h                 \
src/DBCS.cxx                           \
src/DBCS.h                             \
src/Debugging.h                        \
src/Decoration.cxx                     \
src/Decoration.h                       \
src/Document.cxx                       \
src/Document.h                         \
src/EditModel.cxx                      \
src/EditModel.h                        \
src/Editor.cxx                         \
src/Editor.h                           \
src/EditView.cxx                       \
src/EditView.h                         \
src/ElapsedPeriod.h                    \
src/FontQuality.h                      \
src/Geometry.cxx                       \
src/Geometry.h                         \
src/Indicator.cxx                      \
src/Indicator.h                        \
src/KeyMap.cxx                         \
src/KeyMap.h                           \
src/LineMarker.cxx                     \
src/LineMarker.h                       \
src/MarginView.cxx                     \
src/MarginView.h                       \
src/Partitioning.h                     \
src/PerLine.cxx                        \
src/PerLine.h                          \
src/Platform.h                         \
src/PositionCache.cxx                  \
src/PositionCache.h                    \
src/Position.h                         \
src/RESearch.cxx                       \
src/RESearch.h                         \
src/RunStyles.cxx                      \
src/RunStyles.h                        \
src/ScintillaBase.cxx                  \
src/ScintillaBase.h                    \
src/Selection.cxx                      \
src/Selection.h                        \
src/SparseVector.h                     \
src/SplitVector.h                      \
src/Style.cxx                          \
src/Style.h                            \
src/UniConversion.cxx                  \
src/UniConversion.h                    \
src/UniqueString.cxx                   \
src/UniqueString.h                     \
src/ViewStyle.cxx                      \
src/ViewStyle.h                        \
src/XPM.cxx                            \
src/XPM.h

liblexilla_la_CPPFLAGS = $(AM_CPPFLAGS)
liblexilla_la_CPPFLAGS += -I$(srcdir)/lexilla/include -I$(srcdir)/lexilla/lexlib -I$(srcdir)/include
liblexilla_la_SOURCES = $(LEXLIB_SRCS) $(LEXER_SRCS)

libscintilla_la_CPPFLAGS = $(AM_CPPFLAGS)
libscintilla_la_CPPFLAGS += -I$(srcdir)/src -I$(srcdir)/include -I$(srcdir)/lexilla/include
libscintilla_la_SOURCES = $(SRCS)

marshallers: gtk/scintilla-marshal.list
	glib-genmarshal --prefix scintilla_marshal gtk/scintilla-marshal.list --header > gtk/scintilla-marshal.h
	glib-genmarshal --prefix scintilla_marshal gtk/scintilla-marshal.list --body > gtk/scintilla-marshal.c

EXTRA_DIST = gtk/scintilla-marshal.list License.txt README version.txt
EXTRA_DIST += lexilla/License.txt lexilla/version.txt
