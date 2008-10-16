## Process this file with automake to produce Makefile.in
# $Id$

SUBDIRS = include
INCLUDES = -I$(srcdir)/include $(GTK_CFLAGS)
#	-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED \
#	-DGDK_PIXBUF_DEPRECATED \
#	-DGTK_DISABLE_DEPRECATED -DGNOME_DISABLE_DEPRECATED

# regex.c is the GNU regex implementation needed for Windows
EXTRA_DIST = regex.c

noinst_LIBRARIES = libtagmanager.a

if USE_INCLUDED_REGEX
regex_sources = regex.c
else
regex_sources =
endif

libtagmanager_a_SOURCES =\
	general.h\
	keyword.h\
	parsers.h\
	sort.h\
	vstring.h\
	ctags.h\
	entry.h\
	get.h\
	main.h\
	read.h\
	parse.h\
	strlist.h\
	args.c\
	args.h\
	basic.c\
	c.c\
	conf.c\
	css.c\
	diff.c\
	docbook.c\
	fortran.c\
	haskell.c\
	haxe.c\
	html.c\
	js.c\
	lua.c\
	make.c\
	asm.c\
	latex.c\
	lregex.c\
	pascal.c\
	perl.c\
	rest.c\
	ruby.c\
	sql.c\
	php.c\
	python.c\
	tcl.c\
	sh.c\
	vhdl.c\
	ctags.c\
	entry.c\
	get.c\
	keyword.c\
	options.h\
	options.c\
	parse.c\
	read.c\
	sort.c\
	strlist.c\
	vstring.c\
	tm_workspace.c\
	tm_work_object.c\
	tm_source_file.c\
	tm_project.c\
	tm_tag.c\
	tm_symbol.c\
	tm_file_entry.c\
	tm_tagmanager.c \
	$(regex_sources)

