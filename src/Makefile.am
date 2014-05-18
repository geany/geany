## Process this file with automake to produce Makefile.in


EXTRA_DIST = \
	gb.c \
	win32.c win32.h \
	plugindata.h \
	documentprivate.h \
	filetypesprivate.h \
	keybindingsprivate.h \
	pluginprivate.h \
	projectprivate.h \
	makefile.win32

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
	geanyentryaction.c geanyentryaction.h \
	geanymenubuttonaction.c geanymenubuttonaction.h \
	geanyobject.c geanyobject.h \
	geanywraplabel.c geanywraplabel.h \
	gtkcompat.h \
	highlighting.c highlighting.h \
	highlightingmappings.h \
	keybindings.c keybindings.h \
	keyfile.c keyfile.h \
	log.c log.h \
	main.c main.h geany.h \
	msgwindow.c msgwindow.h \
	navqueue.c navqueue.h \
	notebook.c notebook.h \
	plugins.c plugins.h \
	pluginutils.c pluginutils.h \
	prefix.c prefix.h \
	prefs.c prefs.h \
	printing.c printing.h \
	project.c project.h \
	sciwrappers.c sciwrappers.h \
	search.c search.h \
	socket.c socket.h \
	stash.c stash.h \
	support.h \
	symbols.c symbols.h \
	templates.c templates.h \
	toolbar.c toolbar.h \
	tools.c tools.h \
	sidebar.c sidebar.h \
	ui_utils.c ui_utils.h \
	utils.c utils.h


geany_includedir = $(includedir)/geany

# only install headers that define types or macros, not just functions
geany_include_HEADERS = \
	build.h \
	document.h \
	editor.h \
	encodings.h \
	filetypes.h \
	geany.h \
	gtkcompat.h \
	highlighting.h \
	keybindings.h \
	msgwindow.h \
	plugindata.h \
	prefs.h \
	project.h \
	search.h \
	stash.h \
	support.h \
	templates.h \
	toolbar.h \
	ui_utils.h \
	utils.h


AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/scintilla/include \
	-I$(top_srcdir)/tagmanager/src \
	@GTK_CFLAGS@ @GTHREAD_CFLAGS@

# tell automake we have a C++ file so it uses the C++ linker we need for Scintilla
nodist_EXTRA_geany_SOURCES = dummy.cxx


if MINGW
# build Geany for Windows on non-Windows systems (cross-compile)

geany_SOURCES = $(SRCS) win32.c win32.h

geany_LDADD = \
	$(top_builddir)/scintilla/libscintilla.a \
	$(top_builddir)/tagmanager/ctags/libctags.a \
	$(top_builddir)/tagmanager/mio/libmio.a \
	$(top_builddir)/tagmanager/src/libtagmanager.a \
	@GTK_LIBS@ \
	@GTHREAD_LIBS@ \
	$(INTLLIBS) \
	-lole32 -luuid -liberty -lwsock32 \
	geany_private.res

AM_CFLAGS = -DGEANY_DATADIR=\"data\" \
			-DGEANY_DOCDIR=\"\" \
			-DGEANY_LIBDIR=\"\" \
			-DGEANY_LOCALEDIR=\"\" \
			-DGEANY_PREFIX=\"\" \
			-DGEANY_PRIVATE \
			-DGTK \
			-DG_LOG_DOMAIN=\""Geany"\"

geany_LDFLAGS =	-mwindows -mms-bitfields

WINDRES = $(host_alias)-windres

geany_private.res:
	$(WINDRES) -i ../geany_private.rc --input-format=rc -o geany_private.res -O coff;

clean-local:
	rm -f geany_private.res

else
# build Geany for all other platforms

geany_SOURCES = $(SRCS) vte.c vte.h

geany_LDADD = \
	$(top_builddir)/scintilla/libscintilla.a \
	$(top_builddir)/tagmanager/ctags/libctags.a \
	$(top_builddir)/tagmanager/mio/libmio.a \
	$(top_builddir)/tagmanager/src/libtagmanager.a \
	@GTK_LIBS@ \
	@GTHREAD_LIBS@ \
	$(INTLLIBS)

AM_CFLAGS = -DGEANY_DATADIR=\""$(datadir)"\" \
			-DGEANY_DOCDIR=\""$(docdir)"\" \
			-DGEANY_LIBDIR=\""$(libdir)"\" \
			-DGEANY_LOCALEDIR=\""$(localedir)"\" \
			-DGEANY_PREFIX=\""$(prefix)"\" \
			-DGEANY_PRIVATE \
			-DGTK \
			-DG_LOG_DOMAIN=\""Geany"\"

clean-local:

endif

