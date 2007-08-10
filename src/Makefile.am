## Process this file with automake to produce Makefile.in
# $Id$


EXTRA_DIST = images.c gb.c win32.c win32.h plugindata.h

bin_PROGRAMS = geany

SRCS = \
	about.c about.h \
	build.c build.h \
	callbacks.c callbacks.h \
	dialogs.c dialogs.h \
	document.c document.h \
	editor.c editor.h \
	encodings.c encodings.h \
	filetypes.c filetypes.h \
	geanyobject.c geanyobject.h \
	highlighting.c highlighting.h \
	interface.c interface.h \
	keybindings.c keybindings.h \
	keyfile.c keyfile.h \
	main.c main.h geany.h \
	msgwindow.c msgwindow.h \
	navqueue.c navqueue.h \
	notebook.c notebook.h \
	plugins.c plugins.h \
	prefs.c prefs.h \
	project.c project.h \
	sciwrappers.c sciwrappers.h \
	search.c search.h \
	socket.c socket.h \
	support.c support.h \
	symbols.c symbols.h \
	templates.c templates.h \
	tools.c tools.h \
	treeviews.c treeviews.h \
	ui_utils.c ui_utils.h \
	utils.c utils.h

# just to make "make distcheck" happy
noinst_HEADERS = \
	geany.h \
	support.h \
	plugindata.h

if MINGW
# build Geany for Windows
WINDRES = /usr/local/cross-tools/bin/i386-mingw32msvc-windres

geany_SOURCES = $(SRCS) win32.c win32.h
geany_LDADD = ../scintilla/libscintilla.a ../tagmanager/libtagmanager.a -lstdc++ @GTK_LIBS@ \
			$(INTLLIBS) -lgdi32 -limm32 -lshell32 -lole32 -luuid -liberty -lcomdlg32 -lcomctl32 \
			geany_private.res
AM_CFLAGS = -Wall -pipe -mms-bitfields
geany_LDFLAGS =	-mwindows

INCLUDES = -I$(srcdir)/../scintilla/include -I$(srcdir)/../tagmanager/include -DENABLE_NLS \
           -I/usr/local/cross-tools/include @GTK_CFLAGS@

geany_windres.res:
	$(WINDRES) -i ../geany_private.rc --input-format=rc -o geany_private.res -O coff;

clean-local:
	rm -f geany_private.res

else
# build Geany for all other platforms
if IS_COMPILER_GCC
    AM_CFLAGS = -Wall -pipe
else
    AM_CFLAGS =
endif

geany_SOURCES = $(SRCS) vte.c vte.h

if IS_COMPILER_GCC
    geany_LDADD = ../scintilla/libscintilla.a ../tagmanager/libtagmanager.a @GTK_LIBS@ -lstdc++ $(INTLLIBS)
else
    geany_LDADD = ../scintilla/libscintilla.a ../tagmanager/libtagmanager.a @GTK_LIBS@ -lCrun $(INTLLIBS)
endif

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" -DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I$(srcdir)/../scintilla/include -I$(srcdir)/../tagmanager/include @GTK_CFLAGS@

clean-local:


endif
