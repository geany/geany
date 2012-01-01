## Process this file with automake to produce Makefile.in

SUBDIRS = mio include
INCLUDES = -I$(srcdir)/include $(GTK_CFLAGS) -I$(srcdir)/mio -I$(srcdir)

AM_CFLAGS = -DG_LOG_DOMAIN=\""Tagmanager"\"

EXTRA_DIST =\
	makefile.win32

noinst_LIBRARIES = libtagmanager.a

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
	nestlevel.h\
	read.h\
	parse.h\
	strlist.h\
	args.c\
	args.h\
	abc.c\
	basic.c\
	c.c\
	cobol.c\
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
	matlab.c\
	markdown.c\
	objc.c\
	pascal.c\
	perl.c\
	rest.c\
	ruby.c\
	sql.c\
	php.c\
	python.c\
	tcl.c\
	sh.c\
	txt2tags.c\
	vhdl.c\
	verilog.c\
	actionscript.c\
	nsis.c\
	r.c \
	ctags.c\
	entry.c\
	get.c\
	keyword.c\
	nestlevel.c\
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

