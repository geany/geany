AM_CPPFLAGS = \
	-I$(srcdir)/main \
	-I$(srcdir)/parsers \
	-DEXTERNAL_PARSER_LIST_FILE=\"$(top_srcdir)/src/tagmanager/tm_parsers.h\" \
	-DG_LOG_DOMAIN=\"CTags\"
AM_CFLAGS = \
	$(GTK_CFLAGS) \
	@LIBGEANY_CFLAGS@

noinst_LTLIBRARIES = libctags.la

parsers = \
	parsers/geany_abaqus.c \
	parsers/geany_abc.c \
	parsers/geany_asciidoc.c \
	parsers/geany_asm.c \
	parsers/geany_basic.c \
	parsers/geany_bibtex.c \
	parsers/geany_c.c \
	parsers/geany_cobol.c \
	parsers/geany_iniconf.c \
	parsers/geany_css.c \
	parsers/geany_diff.c \
	parsers/geany_docbook.c \
	parsers/geany_erlang.c \
	parsers/geany_flex.c \
	parsers/geany_fortran.c \
	parsers/geany_go.c \
	parsers/geany_haskell.c \
	parsers/geany_haxe.c \
	parsers/geany_html.c \
	parsers/geany_jscript.c \
	parsers/geany_json.c \
	parsers/geany_julia.c \
	parsers/geany_lcpp.c \
	parsers/geany_lcpp.h \
	parsers/geany_lua.c \
	parsers/geany_make.c \
	parsers/geany_markdown.c \
	parsers/geany_matlab.c \
	parsers/geany_nsis.c \
	parsers/geany_objc.c \
	parsers/geany_pascal.c \
	parsers/geany_perl.c \
	parsers/geany_php.c \
	parsers/geany_powershell.c \
	parsers/geany_python.c \
	parsers/geany_r.c \
	parsers/geany_rst.c \
	parsers/geany_ruby.c \
	parsers/geany_rust.c \
	parsers/geany_sh.c \
	parsers/geany_sql.c \
	parsers/geany_tcl.c \
	parsers/geany_tex.c \
	parsers/geany_txt2tags.c \
	parsers/geany_verilog.c \
	parsers/geany_vhdl.c

# skip cmd.c and mini-geany.c which define main()
libctags_la_SOURCES = \
	main/args.c \
	main/args_p.h \
	main/colprint.c \
	main/colprint_p.h \
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
