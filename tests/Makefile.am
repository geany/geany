
SUBDIRS = ctags

AM_CPPFLAGS  = -DGEANY_PRIVATE -DG_LOG_DOMAIN=\""Geany"\" @GTK_CFLAGS@ @GTHREAD_CFLAGS@
AM_CPPFLAGS += -I$(top_srcdir)/src

AM_LDFLAGS = $(GTK_LIBS) $(GTHREAD_LIBS) $(INTLLIBS) -no-install

check_PROGRAMS = test_utils

test_utils_LDADD = $(top_builddir)/src/libgeany.la

TESTS = $(check_PROGRAMS)
