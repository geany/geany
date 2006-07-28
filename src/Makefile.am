## Process this file with automake to produce Makefile.in
# $Id$


EXTRA_DIST = images.c gb.c

bin_PROGRAMS = geany

SRCS = \
	main.c geany.h \
	search.c search.h \
	notebook.c notebook.h \
	keybindings.c keybindings.h \
	templates.c templates.h \
	treeviews.c treeviews.h \
	about.c about.h \
	encodings.c encodings.h \
	keyfile.c keyfile.h \
	prefs.c prefs.h \
	dialogs.c dialogs.h \
	msgwindow.c msgwindow.h \
	build.c build.h \
	filetypes.c filetypes.h \
	highlighting.c highlighting.h \
	sci_cb.c sci_cb.h \
	sciwrappers.c sciwrappers.h \
	document.c document.h \
	utils.c utils.h \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h



if MINGW
# build Geany for Windows
WINDRES = /usr/local/cross-tools/bin/i386-mingw32msvc-windres

geany_SOURCES = $(SRCS) win32.c win32.h
geany_LDADD = ../scintilla/libscintilla.a ../tagmanager/libtagmanager.a -lstdc++ @PACKAGE_LIBS@ \
			$(INTLLIBS) -lgdi32 -limm32 -lshell32 -lole32 -luuid -liberty -lcomdlg32 -lcomctl32 \
			geany_windres.res
AM_CFLAGS = -DGEANY_DEBUG -Wall -pipe -mms-bitfields
#AM_CFLAGS = -DGEANY_DEBUG -Wall -pipe -mms-bitfields -g -O0
geany_LDFLAGS =	-mwindows

INCLUDES = -I../scintilla/include -I../tagmanager/include -DENABLE_NLS \
           -I/usr/local/cross-tools/include @PACKAGE_CFLAGS@

geany_windres.res:
	$(WINDRES) -i ../geany_windres.rc --input-format=rc -o geany_windres.res -O coff;

clean-local:
	rm -f geany_windres.res

else
# build Geany for all other platforms
AM_CFLAGS = -DGEANY_DEBUG -Wall -pipe
#AM_CFLAGS = -DGEANY_DEBUG -Wall -pipe -g -O0
geany_SOURCES = $(SRCS) vte.c vte.h
geany_LDADD = @PACKAGE_LIBS@ -lstdc++ ../scintilla/libscintilla.a ../tagmanager/libtagmanager.a $(INTLLIBS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I../scintilla/include -I../tagmanager/include @PACKAGE_CFLAGS@

clean-local:

endif

#geany_SOURCES = $(SRCS)
#geany_LDADD = @PACKAGE_LIBS@ -lstdc++ ../scintilla/libscintilla.a ../tagmanager/libtagmanager.a $(INTLLIBS)
