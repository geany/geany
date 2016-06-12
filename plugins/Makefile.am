# Adapted from Pidgin's plugins/Makefile.am, thanks

EXTRA_DIST = \
	demoproxytest.px

plugindir = $(libdir)/geany

plugins_includedir = $(includedir)/geany
plugins_include_HEADERS = \
	geanyfunctions.h \
	geanyplugin.h

demoplugin_la_LDFLAGS    = -module -avoid-version -no-undefined
demoproxy_la_LDFLAGS     = -module -avoid-version -no-undefined
classbuilder_la_LDFLAGS  = -module -avoid-version -no-undefined
htmlchars_la_LDFLAGS     = -module -avoid-version -no-undefined
export_la_LDFLAGS        = -module -avoid-version -no-undefined
saveactions_la_LDFLAGS   = -module -avoid-version -no-undefined
filebrowser_la_LDFLAGS   = -module -avoid-version -no-undefined
splitwindow_la_LDFLAGS   = -module -avoid-version -no-undefined

if PLUGINS
# Plugins to be installed
plugin_LTLIBRARIES = \
	classbuilder.la \
	htmlchars.la \
	export.la \
	saveactions.la \
	filebrowser.la \
	splitwindow.la

# Plugins not to be installed
noinst_LTLIBRARIES = \
	demoplugin.la  \
	demoproxy.la

demoplugin_la_SOURCES    = demoplugin.c
demoproxy_la_SOURCES     = demoproxy.c
classbuilder_la_SOURCES  = classbuilder.c
htmlchars_la_SOURCES     = htmlchars.c
export_la_SOURCES        = export.c
saveactions_la_SOURCES   = saveactions.c
filebrowser_la_SOURCES   = filebrowser.c
splitwindow_la_SOURCES   = splitwindow.c

demoplugin_la_CFLAGS    = -DG_LOG_DOMAIN=\""Demoplugin"\" -DLOCALEDIR=\""$(LOCALEDIR)"\"
demoproxy_la_CFLAGS     = -DG_LOG_DOMAIN=\""Demoproxy"\"
classbuilder_la_CFLAGS  = -DG_LOG_DOMAIN=\""Classbuilder"\"
htmlchars_la_CFLAGS     = -DG_LOG_DOMAIN=\""HTMLChars"\"
export_la_CFLAGS        = -DG_LOG_DOMAIN=\""Export"\"
saveactions_la_CFLAGS   = -DG_LOG_DOMAIN=\""SaveActions"\"
filebrowser_la_CFLAGS   = -DG_LOG_DOMAIN=\""FileBrowser"\"
splitwindow_la_CFLAGS   = -DG_LOG_DOMAIN=\""SplitWindow"\"

demoplugin_la_LIBADD    = $(top_builddir)/src/libgeany.la $(GTK_LIBS)
demoproxy_la_LIBADD     = $(top_builddir)/src/libgeany.la $(GTK_LIBS)
classbuilder_la_LIBADD  = $(top_builddir)/src/libgeany.la $(GTK_LIBS)
htmlchars_la_LIBADD     = $(top_builddir)/src/libgeany.la $(GTK_LIBS)
export_la_LIBADD        = $(top_builddir)/src/libgeany.la $(GTK_LIBS) -lm
saveactions_la_LIBADD   = $(top_builddir)/src/libgeany.la $(GTK_LIBS)
filebrowser_la_LIBADD   = $(top_builddir)/src/libgeany.la $(GTK_LIBS)
splitwindow_la_LIBADD   = $(top_builddir)/src/libgeany.la $(GTK_LIBS)

endif # PLUGINS

if MINGW
# FIXME: why is this define different than the non-MINGW one?
AM_CPPFLAGS = -DGEANY_DATADIR=\"data\"
else
AM_CPPFLAGS = -DDATADIR=\"$(datadir)\"
endif

AM_CPPFLAGS += \
	-DGTK \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/tagmanager \
	-I$(top_srcdir)/scintilla/include \
	$(GTK_CFLAGS) \
	$(PLUGIN_CFLAGS)

#
# This part allows people to build their own plugins in here.
# Yes, it's a mess.
#
custom_plugins_cppflags = -DHAVE_CONFIG_H -I$(top_srcdir)
custom_plugins_ldflags = -module -avoid-version
if MINGW
SUFFIXES = .c .dll
.c.dll:
	$(CC) $(AM_CPPFLAGS) $(custom_plugins_cppflags) $(CPPFLAGS) $(CFLAGS) -o $@.o -c $<
	$(CC) -shared $@.o $(GTK_LIBS) $(custom_plugins_ldflags) $(LIBS) $(PLUGIN_LIBS) $(LDFLAGS) -o $@
else
SUFFIXES = .c .so
.c.so:
	$(LIBTOOL) --mode=compile $(CC) $(AM_CPPFLAGS) $(custom_plugins_cppflags) $(CPPFLAGS) $(CFLAGS) -c $< -o tmp$@.lo
	$(LIBTOOL) --mode=link    $(CC) $(CFLAGS) -o libtmp$@.la -rpath $(plugindir) tmp$@.lo $(LIBS) $(LDFLAGS) $(custom_plugins_ldflags) $(PLUGIN_LIBS)
	@rm -f tmp$@.lo tmp$@.o libtmp$@.la
	@cp .libs/libtmp$@.so* $@
	@rm -f .libs/libtmp$@.*
endif
