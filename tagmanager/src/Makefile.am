AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)/.. \
	-I$(srcdir)/../ctags \
	-DG_LOG_DOMAIN=\"Tagmanager\"
AM_CFLAGS = \
	$(GTK_CFLAGS)

EXTRA_DIST = \
	makefile.win32

noinst_LIBRARIES = libtagmanager.a

tagmanager_includedir = $(includedir)/geany/tagmanager
tagmanager_include_HEADERS = \
	tm_file_entry.h \
	tm_project.h \
	tm_source_file.h \
	tm_symbol.h \
	tm_tag.h \
	tm_tagmanager.h \
	tm_work_object.h \
	tm_workspace.h


libtagmanager_a_SOURCES =\
	tm_file_entry.c \
	tm_project.c \
	tm_source_file.c \
	tm_symbol.c \
	tm_tag.c \
	tm_tagmanager.c \
	tm_work_object.c \
	tm_workspace.c
