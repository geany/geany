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

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/scintilla/include \
	-I$(top_srcdir)/tagmanager/src \
	-DGTK \
	-DGEANY_PRIVATE \
	-DG_LOG_DOMAIN=\""Geany"\" \
	@GTK_CFLAGS@ @GTHREAD_CFLAGS@ \
	$(MAC_INTEGRATION_CFLAGS)

bin_PROGRAMS = geany
lib_LTLIBRARIES = libgeany.la

geany_SOURCES = main.c
geany_LDADD = libgeany.la $(GTK_LIBS) $(GTHREAD_LIBS) $(INTLLIBS)

geany_includedir = $(includedir)/geany

geany_include_HEADERS = \
	app.h \
	build.h \
	dialogs.h \
	document.h \
	editor.h \
	encodings.h \
	filetypes.h \
	geany.h \
	gtkcompat.h \
	highlighting.h \
	keybindings.h \
	main.h \
	msgwindow.h \
	navqueue.h \
	plugindata.h \
	pluginutils.h \
	prefs.h \
	project.h \
	sciwrappers.h \
	search.h \
	stash.h \
	support.h \
	symbols.h \
	templates.h \
	toolbar.h \
	ui_utils.h \
	utils.h

libgeany_la_SOURCES = \
	about.c about.h \
	app.h \
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
	libmain.c main.h geany.h \
	msgwindow.c msgwindow.h \
	navqueue.c navqueue.h \
	notebook.c notebook.h \
	osx.c osx.h \
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

libgeany_la_CFLAGS  = $(AM_CPPFLAGS) @LIBGEANY_CFLAGS@
libgeany_la_LDFLAGS = @LIBGEANY_LDFLAGS@

libgeany_la_LIBADD = \
	$(top_builddir)/scintilla/libscintilla.la \
	$(top_builddir)/tagmanager/ctags/libctags.la \
	$(top_builddir)/tagmanager/mio/libmio.la \
	$(top_builddir)/tagmanager/src/libtagmanager.la \
	@GTK_LIBS@ \
	@GTHREAD_LIBS@ \
	$(MAC_INTEGRATION_LIBS) \
	$(INTLLIBS)

# tell automake we have a C++ file so it uses the C++ linker we need for Scintilla
nodist_EXTRA_geany_SOURCES = dummy.cxx
nodist_EXTRA_libgeany_la_SOURCES = dummy1.cxx

CLEANFILES =

if MINGW
# build Geany for Windows (possibly on non-Windows systems -- cross-compile)

AM_CPPFLAGS += \
	-DGEANY_DATADIR=\"data\" \
	-DGEANY_DOCDIR=\"\" \
	-DGEANY_LIBDIR=\"\" \
	-DGEANY_LOCALEDIR=\"\" \
	-DGEANY_PREFIX=\"\"

geany_LDADD += geany_private.res

WINDRES = $(host_alias)-windres

geany_private.res: $(top_srcdir)/geany_private.rc
	$(WINDRES) -i $(top_srcdir)/geany_private.rc --input-format=rc -o $@ -O coff

libgeany_la_SOURCES += win32.c win32.h

libgeany_la_LIBADD  += -lole32 -lwsock32 -lcomdlg32

libgeany_la_LDFLAGS += -Wl,-luuid -mwindows -mms-bitfields -no-undefined

CLEANFILES += geany_private.res

else
# build Geany for all other platforms

AM_CPPFLAGS += \
	-DGEANY_DATADIR=\""$(datadir)"\" \
	-DGEANY_DOCDIR=\""$(docdir)"\" \
	-DGEANY_LIBDIR=\""$(libdir)"\" \
	-DGEANY_LOCALEDIR=\""$(localedir)"\" \
	-DGEANY_PREFIX=\""$(prefix)"\"

libgeany_la_SOURCES += vte.c vte.h

endif

callbacks.c: signallist.i

glade_file=$(top_srcdir)/data/geany.glade

signallist.i: $(glade_file) Makefile
	$(AM_V_GEN)( \
		echo '/* This file is auto-generated, do not edit. */' && \
		$(SED) -n 's/^.*handler="\([^"]\{1,\}\)".*$$/ITEM(\1)/p' "$(glade_file)" \
			| $(SORT) | $(UNIQ) \
	) > $@ || { $(RM) $@ && exit 1; }

CLEANFILES += signallist.i
