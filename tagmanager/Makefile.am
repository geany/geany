## Process this file with automake to produce Makefile.in
SUBDIRS = include
INCLUDES = -Iinclude $(PACKAGE_CFLAGS)
#	-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED \
#	-DGDK_PIXBUF_DEPRECATED \
#	-DGTK_DISABLE_DEPRECATED -DGNOME_DISABLE_DEPRECATED

AM_CFLAGS = -Wall

noinst_LIBRARIES = libtagmanager.a
libtagmanager_a_SOURCES =\
	general.h\
	keyword.h\
	parsers.h\
	sort.h\
	vstring.h\
	args.h\
	conf.c\
	ctags.h\
	entry.h\
	get.h\
	main.h\
	read.h\
	parse.h\
	strlist.h\
	args.c\
	args.h\
	c.c\
	make.c\
	tex.c\
	pascal.c\
	perl.c\
	php.c\
	python.c\
	regex.c\
	sh.c\
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
	tm_tagmanager.c

