noinst_LTLIBRARIES = libmio.la

AM_CPPFLAGS = -DG_LOG_DOMAIN=\"MIO\" #-DMIO_DEBUG
AM_CFLAGS   = $(GTK_CFLAGS) @LIBGEANY_CFLAGS@

libmio_la_SOURCES = mio.c
libmio_la_LDFLAGS = @LIBGEANY_LIBS@

EXTRA_DIST = \
	mio.h \
	mio-file.c \
	mio-memory.c \
	makefile.win32
