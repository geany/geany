
SUBDIRS = ctags

AM_CPPFLAGS  = -DGTK -DGEANY_PRIVATE -DG_LOG_DOMAIN=\""Geany"\"
AM_CPPFLAGS += -I$(top_srcdir)/scintilla/include -I$(top_srcdir)/scintilla/lexilla/include
AM_CPPFLAGS += -I$(top_srcdir)/src/tagmanager -I$(top_srcdir)/src
AM_CFLAGS = $(GTK_CFLAGS)
AM_LDFLAGS = $(GTK_LIBS) $(INTLLIBS) -no-install

check_PROGRAMS = test_utils test_sidebar test_encodings

test_utils_LDADD = $(top_builddir)/src/libgeany.la
test_sidebar_LDADD = $(top_builddir)/src/libgeany.la
test_encodings_LDADD = $(top_builddir)/src/libgeany.la

TESTS = $(check_PROGRAMS)
