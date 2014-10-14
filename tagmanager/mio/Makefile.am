noinst_LTLIBRARIES = libmio.la

AM_CPPFLAGS = -DG_LOG_DOMAIN=\"MIO\" #-DMIO_DEBUG
AM_CFLAGS   = $(GTK_CFLAGS) -fvisibility=hidden

libmio_la_SOURCES = mio.c

EXTRA_DIST = \
	mio.h \
	mio-file.c \
	mio-memory.c \
	makefile.win32
