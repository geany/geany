# Adapted from Pidgin's plugins/Makefile.am, thanks

EXTRA_DIST = \
	makefile.win32 \
	genapi.py

plugindir = $(libdir)/geany

plugins_includedir = $(includedir)/geany
plugins_include_HEADERS = \
	geanyplugin.h \
	geanyfunctions.h

# systems without python should continue to build OK
geanyfunctions.h: genapi.py ../src/plugins.c
	python genapi.py || true

all: geanyfunctions.h


demoplugin_la_LDFLAGS    = -module -avoid-version
classbuilder_la_LDFLAGS  = -module -avoid-version
htmlchars_la_LDFLAGS     = -module -avoid-version
export_la_LDFLAGS        = -module -avoid-version
saveactions_la_LDFLAGS   = -module -avoid-version
filebrowser_la_LDFLAGS   = -module -avoid-version
splitwindow_la_LDFLAGS   = -module -avoid-version

if PLUGINS

if MINGW
# build Geany for Windows on non-Windows systems (cross-compile)
# (this is a little hack'ish and surely can be improved)
DLL_LD_FLAGS = -module -avoid-version
MINGW_CFLAGS = \
			-DGEANY_DATADIR=\"data\" \
			-DHAVE_CONFIG_H \
			-DGTK \
			-I$(top_srcdir) \
			-I$(top_srcdir)/src \
			-I$(top_srcdir)/tagmanager/include \
			-I$(top_srcdir)/scintilla/include \
			$(GTK_CFLAGS) \
			$(PLUGIN_CFLAGS)

.PHONY: all clean

all-local: \
	classbuilder.dll \
	htmlchars.dll \
	export.dll \
	saveactions.dll \
	splitwindow.dll \
	filebrowser.dll

.c.dll:
	$(CC) $(MINGW_CFLAGS) -o $@.o -c $<
	$(CC) -shared $@.o $(GTK_LIBS) $(DLL_LD_FLAGS) -o $@

clean:
	rm -f *.o *.dll

else

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
	demoplugin.la

demoplugin_la_SOURCES    = demoplugin.c
classbuilder_la_SOURCES  = classbuilder.c
htmlchars_la_SOURCES     = htmlchars.c
export_la_SOURCES        = export.c
saveactions_la_SOURCES   = saveactions.c
filebrowser_la_SOURCES   = filebrowser.c
splitwindow_la_SOURCES   = splitwindow.c

demoplugin_la_CFLAGS    = -DG_LOG_DOMAIN=\""Demoplugin"\"
classbuilder_la_CFLAGS  = -DG_LOG_DOMAIN=\""Classbuilder"\"
htmlchars_la_CFLAGS     = -DG_LOG_DOMAIN=\""HTMLChars"\"
export_la_CFLAGS        = -DG_LOG_DOMAIN=\""Export"\"
saveactions_la_CFLAGS   = -DG_LOG_DOMAIN=\""SaveActions"\"
filebrowser_la_CFLAGS   = -DG_LOG_DOMAIN=\""FileBrowser"\"
splitwindow_la_CFLAGS   = -DG_LOG_DOMAIN=\""SplitWindow"\"

demoplugin_la_LIBADD    = $(GTK_LIBS)
classbuilder_la_LIBADD  = $(GTK_LIBS)
htmlchars_la_LIBADD     = $(GTK_LIBS)
export_la_LIBADD        = $(GTK_LIBS)
saveactions_la_LIBADD   = $(GTK_LIBS)
filebrowser_la_LIBADD   = $(GTK_LIBS)
splitwindow_la_LIBADD   = $(GTK_LIBS)

endif # MINGW
endif # PLUGINS

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DGTK \
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
