# Adapted from Pidgin's plugins/Makefile.am, thanks

#EXTRA_DIST = \
	#makefile.win32

if IS_COMPILER_GCC
    AM_CFLAGS = -Wall -pipe
else
    AM_CFLAGS =
endif

plugindir = $(libdir)/geany

demoplugin_la_LDFLAGS    = -module -avoid-version
classbuilder_la_LDFLAGS  = -module -avoid-version
htmlchars_la_LDFLAGS     = -module -avoid-version
export_la_LDFLAGS        = -module -avoid-version

if PLUGINS

# Plugins to be installed
plugin_LTLIBRARIES = \
	demoplugin.la \
	classbuilder.la \
	htmlchars.la \
	export.la

# Plugins not to be installed
#noinst_LTLIBRARIES = \
	#demoplugin.la

demoplugin_la_SOURCES    = demoplugin.c
classbuilder_la_SOURCES  = classbuilder.c
htmlchars_la_SOURCES     = htmlchars.c
export_la_SOURCES        = export.c

demoplugin_la_LIBADD    = $(GTK_LIBS)
classbuilder_la_LIBADD  = $(GTK_LIBS)
htmlchars_la_LIBADD     = $(GTK_LIBS)
export_la_LIBADD        = $(GTK_LIBS)

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
