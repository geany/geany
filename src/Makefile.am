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
	makefile.win32 \
	$(srcdir)/signalconn.c.in

bin_PROGRAMS = geany
lib_LTLIBRARIES = libgeany.la

geany_SOURCES = main.c
geany_LDADD = libgeany.la $(GTK_LIBS) $(GTHREAD_LIBS) $(INTLLIBS)

SRCS = \
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
	pluginexport.h \
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


AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/scintilla/include \
	-I$(top_srcdir)/tagmanager/src \
	@GTK_CFLAGS@ @GTHREAD_CFLAGS@ $(MAC_INTEGRATION_CFLAGS) @LIBGEANY_CFLAGS@

# tell automake we have a C++ file so it uses the C++ linker we need for Scintilla
nodist_EXTRA_geany_SOURCES = dummy.cxx
nodist_EXTRA_libgeany_la_SOURCES = dummy1.cxx

if MINGW
# build Geany for Windows on non-Windows systems (cross-compile)

libgeany_la_SOURCES = $(SRCS) win32.c win32.h

libgeany_la_LIBADD = \
	$(top_builddir)/scintilla/libscintilla.la \
	$(top_builddir)/tagmanager/ctags/libctags.la \
	$(top_builddir)/tagmanager/mio/libmio.la \
	$(top_builddir)/tagmanager/src/libtagmanager.la \
	@GTK_LIBS@ \
	@GTHREAD_LIBS@ \
	$(INTLLIBS) \
	@LIBGEANY_LIBS@ \
	-lole32 -lwsock32 -lcomdlg32

libgeany_la_LDFLAGS = -Wl,-luuid $(AM_LDFLAGS)
geany_LDADD += geany_private.res  -lcomdlg32

AM_CFLAGS = -DGEANY_DATADIR=\"data\" \
			-DGEANY_DOCDIR=\"\" \
			-DGEANY_LIBDIR=\"\" \
			-DGEANY_LOCALEDIR=\"\" \
			-DGEANY_PREFIX=\"\" \
			-DGEANY_PRIVATE \
			-DGTK \
			-DG_LOG_DOMAIN=\""Geany"\"

libgeany_la_LDFLAGS +=	-mwindows -mms-bitfields -no-undefined

WINDRES = $(host_alias)-windres

geany_private.res: $(top_srcdir)/geany_private.rc
	$(WINDRES) -i $(top_srcdir)/geany_private.rc --input-format=rc -o $@ -O coff

clean-local:
	rm -f geany_private.res

else
# build Geany for all other platforms

libgeany_la_SOURCES = $(SRCS) vte.c vte.h

libgeany_la_LIBADD = \
	$(top_builddir)/scintilla/libscintilla.la \
	$(top_builddir)/tagmanager/ctags/libctags.la \
	$(top_builddir)/tagmanager/mio/libmio.la \
	$(top_builddir)/tagmanager/src/libtagmanager.la \
	@GTK_LIBS@ \
	@GTHREAD_LIBS@ \
	$(MAC_INTEGRATION_LIBS) \
	$(INTLLIBS) \
	@LIBGEANY_LIBS@

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

callbacks.c: signalconn.c

glade_file=$(top_srcdir)/data/geany.glade
template_file=$(srcdir)/signalconn.c.in

signalconn.c: $(glade_file) $(template_file)
	$(AM_V_GEN)( \
		echo '/* This file is auto-generated, do not edit. */' && \
		$(SED) -n '/@callback_map@/q;p' "$(template_file)" && \
		$(SED) -n 's/^.*handler="\([^"]\+\)".*$$/\tg_hash_table_insert(hash, (gpointer) "\1", G_CALLBACK(\1));/p' "$(glade_file)" \
			| $(SORT) | $(UNIQ) && \
		$(SED) -n '/@callback_map@/{:l;n;p;b l}' "$(template_file)" \
	) > $@ || { $(RM) $@ && exit 1; }

CLEANFILES = signalconn.c
