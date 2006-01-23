## Process this file with automake to produce Makefile.in
# $Id$

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-I../scintilla/include \
	-I../tagmanager/include \
	@PACKAGE_CFLAGS@

EXTRA_DIST = images.c gb.c

bin_PROGRAMS = geany

geany_SOURCES = \
	main.c geany.h \
	templates.c templates.h \
	treeviews.c treeviews.h \
	about.c about.h \
	encodings.c encodings.h \
	keyfile.c keyfile.h \
	prefs.c prefs.h \
	dialogs.c dialogs.h \
	msgwindow.c msgwindow.h \
	build.c build.h \
	win32.c win32.h \
	filetypes.c filetypes.h \
	highlighting.c highlighting.h \
	sci_cb.c sci_cb.h \
	sciwrappers.c sciwrappers.h \
	document.c document.h \
	utils.c utils.h \
	vte.c vte.h \
	support.c support.h \
	interface.c interface.h \
	callbacks.c callbacks.h

#AM_CFLAGS = -Wall -pipe
AM_CFLAGS = -DGEANY_DEBUG -Wall -pipe
#AM_CFLAGS = -DGEANY_DEBUG -g -Wall -pipe
geany_LDADD = @PACKAGE_LIBS@ -lstdc++ ../scintilla/libscintilla.a ../tagmanager/libtagmanager.a $(INTLLIBS)
