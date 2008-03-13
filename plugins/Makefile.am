# Adapted from Pidgin's plugins/Makefile.am, thanks

EXTRA_DIST = \
	makefile.win32 \
	pluginmacros.h

plugindir = $(libdir)/geany

plugins_includedir = $(includedir)/geany/
plugins_include_HEADERS = pluginmacros.h

demoplugin_la_LDFLAGS    = -module -avoid-version
classbuilder_la_LDFLAGS  = -module -avoid-version
htmlchars_la_LDFLAGS     = -module -avoid-version
export_la_LDFLAGS        = -module -avoid-version
vcdiff_la_LDFLAGS        = -module -avoid-version
autosave_la_LDFLAGS      = -module -avoid-version
filebrowser_la_LDFLAGS   = -module -avoid-version

if PLUGINS

# Plugins to be installed
plugin_LTLIBRARIES = \
	classbuilder.la \
	htmlchars.la \
	export.la \
	vcdiff.la \
	autosave.la \
	filebrowser.la

# Plugins not to be installed
noinst_LTLIBRARIES = \
	demoplugin.la

demoplugin_la_SOURCES    = demoplugin.c
classbuilder_la_SOURCES  = classbuilder.c
htmlchars_la_SOURCES     = htmlchars.c
export_la_SOURCES        = export.c
vcdiff_la_SOURCES        = vcdiff.c
autosave_la_SOURCES      = autosave.c
filebrowser_la_SOURCES   = filebrowser.c

demoplugin_la_LIBADD    = $(GTK_LIBS)
classbuilder_la_LIBADD  = $(GTK_LIBS)
htmlchars_la_LIBADD     = $(GTK_LIBS)
export_la_LIBADD        = $(GTK_LIBS)
vcdiff_la_LIBADD        = $(GTK_LIBS)
autosave_la_LIBADD      = $(GTK_LIBS)
filebrowser_la_LIBADD   = $(GTK_LIBS)

endif # PLUGINS

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/tagmanager/include \
	-I$(top_srcdir)/scintilla/include \
	$(GTK_CFLAGS) \
	$(PLUGIN_CFLAGS)

#
# This part allows people to build their own plugins in here.
# Yes, it's a mess.
#
SUFFIXES = .c .so
.c.so:
	$(LIBTOOL) --mode=compile $(CC) -DHAVE_CONFIG_H -I$(top_srcdir) $(AM_CPPFLAGS) $(CFLAGS) -c $< -o tmp$@.lo $(PLUGIN_CFLAGS)
	$(LIBTOOL) --mode=link    $(CC) $(CFLAGS) -o libtmp$@.la -rpath $(plugindir) tmp$@.lo $(LIBS) $(LDFLAGS) -module -avoid-version $(PLUGIN_LIBS)
	@rm -f tmp$@.lo tmp$@.o libtmp$@.la
	@cp .libs/libtmp$@.so* $@
	@rm -f .libs/libtmp$@.*
