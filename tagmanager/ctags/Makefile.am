AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(srcdir)/.. \
	-DG_LOG_DOMAIN=\"CTags\"
AM_CFLAGS = \
	$(GTK_CFLAGS) \
	@LIBGEANY_CFLAGS@

EXTRA_DIST = \
	makefile.win32

noinst_LTLIBRARIES = libctags.la

parsers = \
	abaqus.c \
	abc.c \
	actionscript.c \
	asciidoc.c \
	asm.c \
	basic.c \
	c.c \
	cobol.c \
	conf.c \
	css.c \
	diff.c \
	docbook.c \
	fortran.c \
	haskell.c \
	haxe.c \
	html.c \
	js.c \
	latex.c \
	lregex.c \
	lua.c \
	make.c \
	markdown.c \
	matlab.c \
	nsis.c \
	objc.c \
	pascal.c \
	perl.c \
	php.c \
	python.c \
	r.c \
	rest.c \
	ruby.c \
	rust.c \
	sh.c \
	sql.c \
	tcl.c \
	txt2tags.c \
	verilog.c \
	vhdl.c

libctags_la_SOURCES = \
	args.c \
	args.h \
	ctags.c \
	ctags.h \
	entry.c \
	entry.h \
	general.h \
	get.c \
	get.h \
	keyword.c \
	keyword.h \
	main.h \
	nestlevel.c \
	nestlevel.h \
	options.c \
	options.h \
	parse.c \
	parse.h \
	parsers.h \
	read.c \
	read.h \
	sort.c \
	sort.h \
	strlist.c \
	strlist.h \
	vstring.c \
	vstring.h \
	$(parsers)

libctags_la_LDFLAGS = @LIBGEANY_LIBS@
