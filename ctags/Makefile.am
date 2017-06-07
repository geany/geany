AM_CPPFLAGS = \
	-I$(srcdir)/main \
	-I$(srcdir)/parsers \
	-DG_LOG_DOMAIN=\"CTags\"
AM_CFLAGS = \
	$(GTK_CFLAGS) \
	@LIBGEANY_CFLAGS@

noinst_LTLIBRARIES = libctags.la

parsers = \
	parsers/abaqus.c \
	parsers/abc.c \
	parsers/actionscript.c \
	parsers/asciidoc.c \
	parsers/asm.c \
	parsers/basic.c \
	parsers/c.c \
	parsers/cobol.c \
	parsers/conf.c \
	parsers/css.c \
	parsers/diff.c \
	parsers/docbook.c \
	parsers/erlang.c \
	parsers/fortran.c \
	parsers/go.c \
	parsers/haskell.c \
	parsers/haxe.c \
	parsers/html.c \
	parsers/jscript.c \
	parsers/json.c \
	parsers/latex.c \
	parsers/lua.c \
	parsers/make.c \
	parsers/markdown.c \
	parsers/matlab.c \
	parsers/nsis.c \
	parsers/objc.c \
	parsers/pascal.c \
	parsers/perl.c \
	parsers/php.c \
	parsers/powershell.c \
	parsers/python.c \
	parsers/r.c \
	parsers/rest.c \
	parsers/ruby.c \
	parsers/rust.c \
	parsers/sh.c \
	parsers/sql.c \
	parsers/tcl.c \
	parsers/txt2tags.c \
	parsers/verilog.c \
	parsers/vhdl.c

libctags_la_SOURCES = \
	main/args.c \
	main/args.h \
	main/ctags.h \
	main/debug.h \
	main/entry.c \
	main/entry.h \
	main/general.h \
	main/keyword.c \
	main/keyword.h \
	main/kind.h \
	main/lcpp.c \
	main/lcpp.h \
	main/lregex.c \
	main/main.c \
	main/main.h \
	main/mio.c \
	main/mio.h \
	main/nestlevel.c \
	main/nestlevel.h \
	main/options.c \
	main/options.h \
	main/parse.c \
	main/parse.h \
	main/parsers.h \
	main/read.c \
	main/read.h \
	main/routines.c \
	main/routines.h \
	main/sort.c \
	main/sort.h \
	main/strlist.c \
	main/strlist.h \
	main/vstring.c \
	main/vstring.h \
	main/xtag.h \
	main/xtag.c \
	$(parsers)
