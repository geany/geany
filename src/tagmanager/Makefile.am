AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(top_srcdir)/ctags/main \
	-DGEANY_PRIVATE \
	-DG_LOG_DOMAIN=\"Tagmanager\"
AM_CFLAGS = \
	$(GTK_CFLAGS) \
	@LIBGEANY_CFLAGS@

noinst_LTLIBRARIES = libtagmanager.la

tagmanager_includedir = $(includedir)/geany/tagmanager
tagmanager_include_HEADERS = \
	tm_source_file.h \
	tm_tag.h \
	tm_workspace.h \
	tm_parser.h


libtagmanager_la_SOURCES =\
	tm_parser.h \
	tm_parser.c \
	tm_source_file.h \
	tm_source_file.c \
	tm_tag.h \
	tm_tag.c \
	tm_workspace.h \
	tm_workspace.c \
	tm_ctags_wrappers.h \
	tm_ctags_wrappers.c

libtagmanager_la_LIBADD = $(top_builddir)/ctags/libctags.la $(GTK_LIBS)
