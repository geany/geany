AM_CPPFLAGS = \
	-I$(srcdir)/main \
	-I$(srcdir)/parsers \
	-I$(srcdir)/dsl \
	-DEXTERNAL_PARSER_LIST_FILE=\"$(top_srcdir)/src/tagmanager/tm_parsers.h\" \
	-DG_LOG_DOMAIN=\"CTags\"
AM_CFLAGS = \
	$(GTK_CFLAGS) \
	@LIBGEANY_CFLAGS@

noinst_LTLIBRARIES = libctags.la

parsers = \
	parsers/cxx/cxx.c \
	parsers/cxx/cxx_debug.c \
	parsers/cxx/cxx_debug.h \
	parsers/cxx/cxx_debug_type.c \
	parsers/cxx/cxx_keyword.c \
	parsers/cxx/cxx_keyword.h \
	parsers/cxx/cxx_parser_block.c \
	parsers/cxx/cxx_parser.c \
	parsers/cxx/cxx_parser_function.c \
	parsers/cxx/cxx_parser.h \
	parsers/cxx/cxx_parser_internal.h \
	parsers/cxx/cxx_parser_lambda.c \
	parsers/cxx/cxx_parser_namespace.c \
	parsers/cxx/cxx_parser_template.c \
	parsers/cxx/cxx_parser_tokenizer.c \
	parsers/cxx/cxx_parser_typedef.c \
	parsers/cxx/cxx_parser_using.c \
	parsers/cxx/cxx_parser_variable.c \
	parsers/cxx/cxx_qtmoc.c \
	parsers/cxx/cxx_scope.c \
	parsers/cxx/cxx_scope.h \
	parsers/cxx/cxx_subparser.c \
	parsers/cxx/cxx_subparser.h \
	parsers/cxx/cxx_subparser_internal.h \
	parsers/cxx/cxx_tag.c \
	parsers/cxx/cxx_tag.h \
	parsers/cxx/cxx_token.c \
	parsers/cxx/cxx_token_chain.c \
	parsers/cxx/cxx_token_chain.h \
	parsers/cxx/cxx_token.h \
	parsers/cpreprocessor.c \
	parsers/cpreprocessor.h \
	parsers/abaqus.c \
	parsers/abc.c \
	parsers/ada.c \
	parsers/asciidoc.c \
	parsers/asm.c \
	parsers/autoit.c \
	parsers/basic.c \
	parsers/bibtex.c \
	parsers/geany_c.c \
	parsers/clojure.c \
	parsers/cobol.c \
	parsers/iniconf.c \
	parsers/iniconf.h \
	parsers/css.c \
	parsers/diff.c \
	parsers/geany_docbook.c \
	parsers/dosbatch.c \
	parsers/erlang.c \
	parsers/flex.c \
	parsers/fortran.c \
	parsers/gdscript.c \
	parsers/go.c \
	parsers/haskell.c \
	parsers/haxe.c \
	parsers/html.c \
	parsers/jscript.c \
	parsers/json.c \
	parsers/julia.c \
	parsers/geany_lcpp.c \
	parsers/geany_lcpp.h \
	parsers/lisp.c \
	parsers/lua.c \
	parsers/make.c \
	parsers/make.h \
	parsers/markdown.c \
	parsers/markdown.h \
	parsers/geany_matlab.c \
	parsers/nsis.c \
	parsers/objc.c \
	parsers/pascal.c \
	parsers/perl.c \
	parsers/perl.h \
	parsers/php.c \
	parsers/powershell.c \
	parsers/python.c \
	parsers/r.c \
	parsers/r.h \
	parsers/rst.c \
	parsers/ruby.c \
	parsers/rust.c \
	parsers/sh.c \
	parsers/sql.c \
	parsers/tcl.c \
	parsers/tcl.h \
	parsers/tcloo.c \
	parsers/tex.c \
	parsers/tex.h \
	parsers/txt2tags.c \
	parsers/typescript.c \
	parsers/verilog.c \
	parsers/vhdl.c

# skip cmd.c and mini-geany.c which define main()
# also skip lregex-pcre2.c which we don't use
libctags_la_SOURCES = \
	dsl/optscript.c \
	dsl/optscript.h \
	dsl/es.c \
	dsl/es.h \
	main/args.c \
	main/args_p.h \
	main/colprint.c \
	main/colprint_p.h \
	main/CommonPrelude.c \
	main/ctags.h \
	main/debug.c \
	main/debug.h \
	main/dependency.c \
	main/dependency.h \
	main/dependency_p.h \
	main/e_msoft.h \
	main/entry.c \
	main/entry.h \
	main/entry_p.h \
	main/entry_private.c \
	main/error.c \
	main/error_p.h \
	main/field.c \
	main/field.h \
	main/field_p.h \
	main/flags.c \
	main/flags_p.h \
	main/fmt.c \
	main/fmt_p.h \
	main/gcc-attr.h \
	main/general.h \
	main/gvars.h \
	main/htable.c \
	main/htable.h \
	main/inline.h \
	main/interactive_p.h \
	main/keyword.c \
	main/keyword.h \
	main/keyword_p.h \
	main/kind.c \
	main/kind.h \
	main/kind_p.h \
	main/lregex.c \
	main/lregex.h \
	main/lregex_p.h \
	main/lregex-default.c \
	main/lxpath.c \
	main/lxpath.h \
	main/lxpath_p.h \
	main/main.c \
	main/main_p.h \
	main/mbcs.c \
	main/mbcs.h \
	main/mbcs_p.h \
	main/mio.c \
	main/mio.h \
	main/nestlevel.c \
	main/nestlevel.h \
	main/numarray.c \
	main/numarray.h \
	main/objpool.c \
	main/objpool.h \
	main/options.c \
	main/options.h \
	main/options_p.h \
	main/param.c \
	main/param.h \
	main/param_p.h \
	main/parse.c \
	main/parse.h \
	main/parse_p.h \
	main/parsers_p.h \
	main/portable-dirent_p.h \
	main/portable-scandir.c \
	main/promise.c \
	main/promise.h \
	main/promise_p.h \
	main/ptag.c \
	main/ptag_p.h \
	main/ptrarray.c \
	main/ptrarray.h \
	main/rbtree.c \
	main/rbtree.h \
	main/read.c \
	main/read.h \
	main/read_p.h \
	main/repoinfo.c \
	main/repoinfo.h \
	main/routines.c \
	main/routines.h \
	main/routines_p.h \
	main/script.c \
	main/script_p.h \
	main/seccomp.c \
	main/selectors.c \
	main/selectors.h \
	main/sort.c \
	main/sort_p.h \
	main/stats.c \
	main/stats_p.h \
	main/strlist.c \
	main/strlist.h \
	main/subparser.h \
	main/subparser_p.h \
	main/tokeninfo.c \
	main/tokeninfo.h \
	main/trace.c \
	main/trace.h \
	main/trashbox.c \
	main/trashbox.h \
	main/trashbox_p.h \
	main/types.h \
	main/unwindi.c \
	main/unwindi.h \
	main/vstring.c \
	main/vstring.h \
	main/writer-ctags.c \
	main/writer-etags.c \
	main/writer-json.c \
	main/writer-xref.c \
	main/writer.c \
	main/writer_p.h \
	main/xtag.c \
	main/xtag.h \
	main/xtag_p.h \
	$(parsers)

libctags_la_LIBADD =

# build bundled GNU regex if needed
if USE_BUNDLED_REGEX
noinst_LTLIBRARIES += libgnu_regex.la
libgnu_regex_la_SOURCES = \
	gnu_regex/regex.c \
	gnu_regex/regex.h
# regex.c includes other sources we have to distribute
EXTRA_libgnu_regex_la_SOURCES = \
	gnu_regex/regcomp.c \
	gnu_regex/regex.c \
	gnu_regex/regex.h \
	gnu_regex/regex_internal.c \
	gnu_regex/regex_internal.h \
	gnu_regex/regexec.c
libgnu_regex_la_CPPFLAGS = -D__USE_GNU
EXTRA_DIST = \
	gnu_regex/README.txt

libctags_la_LIBADD += libgnu_regex.la
AM_CPPFLAGS += -I$(srcdir)/gnu_regex
endif

# build bundled fnmatch if needed
if USE_BUNDLED_FNMATCH
noinst_LTLIBRARIES += libfnmatch.la
libfnmatch_la_SOURCES = \
	fnmatch/fnmatch.c \
	fnmatch/fnmatch.h

libctags_la_LIBADD += libfnmatch.la
AM_CPPFLAGS += -I$(srcdir)/fnmatch
endif
