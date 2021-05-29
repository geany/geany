project('geany', 'c', 'cpp', version: '1.38')

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')
prefix = get_option('prefix')

deps_in = [
	['glib-2.0',    '2.32'],
	['gmodule-2.0', '2.32'],
	['gtk+-3.0',    '3.0']
]

deps = []
deps_for_pc = ''
foreach dep : deps_in
	deps += [dependency(dep[0], version: '>= ' + dep[1])]
	deps_for_pc += ' ' + dep[0] + ' >= ' + dep[1]
endforeach

glib = deps[0]

cdata = configuration_data()
check_headers = [
  'dlfcn.h',
  'errno.h',
  'fcntl.h',
  'glob.h',
  'inttypes.h',
  'limits.h',
  'locale.h',
  'memory.h',
  'stdint.h',
  'stdlib.h',
  'strings.h',
  'string.h',
  'stdbool.h',
  'sys/stat.h',
  'sys/time.h',
  'sys/types.h',
  'unistd.h',
]

check_functions = [
  ['gnu_get_libc_version', '#include<gnu/libc-version.h>'],
  ['fnmatch', '#include <fnmatch.h>'],
  ['ftruncate', '#include <unistd.h>'],
  ['mkstemp', '#include <stdlib.h>'],
  ['regcomp', '#include <regex.h>'],
  ['socket', '#include <sys/socket.h>'],
  ['strerror', '#include <string.h>'],
  ['strstr', '#include <string.h>'],
]

foreach h : check_headers
  define = 'HAVE_' + h.underscorify().to_upper()
  cdata.set(define, cc.has_header(h))
endforeach

foreach f : check_functions
  define = 'HAVE_' + f.get(0).underscorify().to_upper()
  cdata.set(define, cc.has_function(f.get(0), prefix : f.get(1)))
endforeach

cdata.set_quoted('PACKAGE', 'geany')
cdata.set_quoted('PACKAGE_BUGREPORT', 'https://github.com/geany/geany/issues')
cdata.set_quoted('PACKAGE_NAME','Geany')
cdata.set_quoted('PACKAGE_STRING', 'Geany ' + meson.project_version())
cdata.set_quoted('PACKAGE_VERSION', meson.project_version())
cdata.set('GETTEXT_PACKAGE', 'PACKAGE')
cdata.set('VERSION', meson.project_version())
cdata.set('ENABLE_NLS', true)
cdata.set('HAVE_VTE', get_option('vte'))
cdata.set('HAVE_PLUGINS', get_option('plugins'))
cdata.set('GEANY_DATA_DIR', join_paths(prefix, get_option('datadir'), 'geany'))
cdata.set('GEANY_DOC_DIR', join_paths(prefix, get_option('datadir'), 'doc', 'geany'))
cdata.set('top_srcdir', meson.source_root())
cdata.set('top_builddir', meson.build_root())

# for geany.pc (adapted from GTK+)
pcconf = cdata
pcconf.set('DEPENDENCIES', deps_for_pc)
pcconf.set('prefix', get_option('prefix'))
pcconf.set('libdir', join_paths('${exec_prefix}', get_option('libdir')))
pcconf.set('includedir', join_paths('${prefix}', get_option('includedir')))
pcconf.set('datarootdir', join_paths('${prefix}', get_option('datadir')))
# actually constant, just match autconf
pcconf.set('exec_prefix', '${prefix}')
pcconf.set('datadir', '${datarootdir}')
pcconf.set('localedir', join_paths('${datarootdir}', 'locale'))

# needed programs
sh = find_program('sh')
cp = find_program('cp')
ln = find_program('ln')
# python is not strictly required but present anyway (due to meson)
python = find_program('python3', 'python')
# These two are truly optional
rst2html = find_program('rst2html', required: false)
rst2pdf = find_program('rst2pdf', required: false)
git = find_program('git', required: false)
if git.found()
	ret = run_command(git, 'rev-parse', '--short', '--revs-only', 'HEAD')
	cdata.set_quoted('REVISION', ret.stdout().strip())
else
	cdata.set_quoted('REVISION', '-1')
endif

configure_file(input : 'config.h.meson',
  output : 'config.h',
  configuration : cdata)

configure_file(input: 'geany.pc.in',
               install: true,
               install_dir: join_paths(prefix, get_option('libdir'), 'pkgconfig'),
               output: 'geany.pc',
               configuration: pcconf)

# CFLAGS for basic stuff that only depends on libc
basic_cflags  = [ '-DHAVE_CONFIG_H=1', '-O2' ]
if cdata.get('HAVE_GNU_GET_LIBC_VERSION')
    basic_cflags += '-D_GNU_SOURCE'
endif

# CFLAGS common between Geany and bundled plugins
def_cflags = basic_cflags + [ '-DGTK' ]
# we're using lots of deprecated stuff with gtk3 (often because there is no proper alternative!)
def_cflags += '-Wno-deprecated-declarations'
def_cflags += '-DGDK_DISABLE_DEPRECATION_WARNINGS'
def_cflags += '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_32'
def_cflags += '-DGEANY_PREFIX="@0@"'.format(get_option('prefix'))
foreach d : [ 'includedir', 'libdir', 'libexecdir', 'datadir', 'localedir' ]
  def_cflags += '-DGEANY_@0@="@1@"'.format(d.underscorify().to_upper(), join_paths(prefix, get_option(d)))
endforeach
def_cflags += '-DGEANY_DOCDIR="@0@"'.format(join_paths(prefix, get_option('datadir'), 'doc'))

# CFLAGS for everything else, i.e. most of Geany
geany_cflags = def_cflags
have_gcc4_visibility = cc.has_argument('-fvisibility=hidden')
geany_cflags += '-DGEANY_PRIVATE'
if target_machine.system() == 'windows'
  geany_cflags += '-DGEANY_EXPORT_SYMBOL="__declspec(dllexport)"'
elif have_gcc4_visibility
  geany_cflags += '-fvisibility=hidden'
  geany_cflags += '-DGEANY_EXPORT_SYMBOL=__attribute__((visibility("default")))'
endif
geany_cflags += '-DGEANY_API_SYMBOL=GEANY_EXPORT_SYMBOL'

iregex = include_directories('ctags/gnu_regex')
ifnmatch = include_directories('ctags/fnmatch')
ictags = include_directories('ctags/main', 'ctags/parsers')
itagmanager = include_directories('src/tagmanager')
iscintilla = include_directories('scintilla/include')
igeany = include_directories('src')

install_headers(
    'scintilla/include/Compat.h',
    'scintilla/include/SciLexer.h',
    'scintilla/include/Scintilla.h',
    'scintilla/include/Scintilla.iface',
    'scintilla/include/ScintillaWidget.h',
    'scintilla/include/Sci_Position.h',
    subdir: 'geany/scintilla')

scintilla = static_library('scintilla',
    'scintilla/gtk/Converter.h',
    'scintilla/gtk/PlatGTK.cxx',
    'scintilla/gtk/ScintillaGTK.cxx',
    'scintilla/gtk/ScintillaGTK.h',
    'scintilla/gtk/ScintillaGTKAccessible.cxx',
    'scintilla/gtk/ScintillaGTKAccessible.h',
    'scintilla/gtk/scintilla-marshal.c',
    'scintilla/gtk/scintilla-marshal.h',
    'scintilla/lexlib/Accessor.cxx',
    'scintilla/lexlib/Accessor.h',
    'scintilla/lexlib/CatalogueModules.h',
    'scintilla/lexlib/CharacterCategory.cxx',
    'scintilla/lexlib/CharacterCategory.h',
    'scintilla/lexlib/CharacterSet.cxx',
    'scintilla/lexlib/CharacterSet.h',
    'scintilla/lexlib/DefaultLexer.cxx',
    'scintilla/lexlib/DefaultLexer.h',
    'scintilla/lexlib/LexAccessor.h',
    'scintilla/lexlib/LexerBase.cxx',
    'scintilla/lexlib/LexerBase.h',
    'scintilla/lexlib/LexerModule.cxx',
    'scintilla/lexlib/LexerModule.h',
    'scintilla/lexlib/LexerNoExceptions.cxx',
    'scintilla/lexlib/LexerNoExceptions.h',
    'scintilla/lexlib/LexerSimple.cxx',
    'scintilla/lexlib/LexerSimple.h',
    'scintilla/lexlib/OptionSet.h',
    'scintilla/lexlib/PropSetSimple.cxx',
    'scintilla/lexlib/PropSetSimple.h',
    'scintilla/lexlib/SparseState.h',
    'scintilla/lexlib/StringCopy.h',
    'scintilla/lexlib/StyleContext.cxx',
    'scintilla/lexlib/StyleContext.h',
    'scintilla/lexlib/SubStyles.h',
    'scintilla/lexlib/WordList.cxx',
    'scintilla/lexlib/WordList.h',
    'scintilla/src/AutoComplete.cxx',
    'scintilla/src/AutoComplete.h',
    'scintilla/src/CallTip.cxx',
    'scintilla/src/CallTip.h',
    'scintilla/src/CaseConvert.cxx',
    'scintilla/src/CaseConvert.h',
    'scintilla/src/CaseFolder.cxx',
    'scintilla/src/CaseFolder.h',
    'scintilla/src/Catalogue.cxx',
    'scintilla/src/Catalogue.h',
    'scintilla/src/CellBuffer.cxx',
    'scintilla/src/CellBuffer.h',
    'scintilla/src/CharClassify.cxx',
    'scintilla/src/CharClassify.h',
    'scintilla/src/ContractionState.cxx',
    'scintilla/src/ContractionState.h',
    'scintilla/src/DBCS.cxx',
    'scintilla/src/DBCS.h',
    'scintilla/src/Decoration.cxx',
    'scintilla/src/Decoration.h',
    'scintilla/src/Document.cxx',
    'scintilla/src/Document.h',
    'scintilla/src/Editor.cxx',
    'scintilla/src/Editor.h',
    'scintilla/src/EditModel.cxx',
    'scintilla/src/EditModel.h',
    'scintilla/src/EditView.cxx',
    'scintilla/src/EditView.h',
    'scintilla/src/ElapsedPeriod.h',
    'scintilla/src/ExternalLexer.cxx',
    'scintilla/src/ExternalLexer.h',
    'scintilla/src/FontQuality.h',
    'scintilla/src/Indicator.cxx',
    'scintilla/src/Indicator.h',
    'scintilla/src/IntegerRectangle.h',
    'scintilla/src/KeyMap.cxx',
    'scintilla/src/KeyMap.h',
    'scintilla/src/LineMarker.cxx',
    'scintilla/src/LineMarker.h',
    'scintilla/src/MarginView.cxx',
    'scintilla/src/MarginView.h',
    'scintilla/src/Partitioning.h',
    'scintilla/src/PerLine.cxx',
    'scintilla/src/PerLine.h',
    'scintilla/src/Position.h',
    'scintilla/src/PositionCache.cxx',
    'scintilla/src/PositionCache.h',
    'scintilla/src/RESearch.cxx',
    'scintilla/src/RESearch.h',
    'scintilla/src/RunStyles.cxx',
    'scintilla/src/RunStyles.h',
    'scintilla/src/SVector.h',
    'scintilla/src/ScintillaBase.cxx',
    'scintilla/src/ScintillaBase.h',
    'scintilla/src/Selection.cxx',
    'scintilla/src/Selection.h',
    'scintilla/src/SparseVector.h',
    'scintilla/src/SplitVector.h',
    'scintilla/src/Style.cxx',
    'scintilla/src/Style.h',
    'scintilla/src/UniConversion.cxx',
    'scintilla/src/UniConversion.h',
    'scintilla/src/UnicodeFromUTF8.h',
    'scintilla/src/UniqueString.cxx',
    'scintilla/src/UniqueString.h',
    'scintilla/src/ViewStyle.cxx',
    'scintilla/src/ViewStyle.h',
    'scintilla/src/XPM.cxx',
    'scintilla/src/XPM.h',
    'scintilla/lexers/LexAbaqus.cxx',
    'scintilla/lexers/LexAda.cxx',
    'scintilla/lexers/LexAsm.cxx',
    'scintilla/lexers/LexBash.cxx',
    'scintilla/lexers/LexBasic.cxx',
    'scintilla/lexers/LexBatch.cxx',
    'scintilla/lexers/LexCOBOL.cxx',
    'scintilla/lexers/LexCPP.cxx',
    'scintilla/lexers/LexCSS.cxx',
    'scintilla/lexers/LexCaml.cxx',
    'scintilla/lexers/LexCmake.cxx',
    'scintilla/lexers/LexCoffeeScript.cxx',
    'scintilla/lexers/LexD.cxx',
    'scintilla/lexers/LexDiff.cxx',
    'scintilla/lexers/LexErlang.cxx',
    'scintilla/lexers/LexForth.cxx',
    'scintilla/lexers/LexFortran.cxx',
    'scintilla/lexers/LexHTML.cxx',
    'scintilla/lexers/LexHaskell.cxx',
    'scintilla/lexers/LexLaTeX.cxx',
    'scintilla/lexers/LexLisp.cxx',
    'scintilla/lexers/LexLua.cxx',
    'scintilla/lexers/LexMake.cxx',
    'scintilla/lexers/LexMarkdown.cxx',
    'scintilla/lexers/LexMatlab.cxx',
    'scintilla/lexers/LexNsis.cxx',
    'scintilla/lexers/LexNull.cxx',
    'scintilla/lexers/LexPascal.cxx',
    'scintilla/lexers/LexPerl.cxx',
    'scintilla/lexers/LexPowerShell.cxx',
    'scintilla/lexers/LexProps.cxx',
    'scintilla/lexers/LexPython.cxx',
    'scintilla/lexers/LexPO.cxx',
    'scintilla/lexers/LexR.cxx',
    'scintilla/lexers/LexRuby.cxx',
    'scintilla/lexers/LexRust.cxx',
    'scintilla/lexers/LexSmalltalk.cxx',
    'scintilla/lexers/LexSQL.cxx',
    'scintilla/lexers/LexTCL.cxx',
    'scintilla/lexers/LexTxt2tags.cxx',
    'scintilla/lexers/LexVHDL.cxx',
    'scintilla/lexers/LexVerilog.cxx',
    'scintilla/lexers/LexYAML.cxx',
    cpp_args: geany_cflags + [ '-Wno-non-virtual-dtor', '-DNDEBUG', '-DSCI_LEXER', '-DNO_CXX11_REGEX' ],
    dependencies: deps,
    include_directories: [iscintilla,
                          include_directories('scintilla/include',
                                              'scintilla/src',
                                              'scintilla/lexlib')]
)
dep_scintilla = declare_dependency(link_with: scintilla,
                                   include_directories: include_directories('scintilla/include'))

if cdata.get('HAVE_REGCOMP')
    dep_regex = dependency('', required: false)
else
    # use regcomp bundled with ctags
    regex = static_library('regex',
        'ctags/gnu_regex/regex.c',
        'ctags/gnu_regex/regex.h',
        c_args: basic_cflags
    )
    dep_regex = declare_dependency(link_with: [regex], include_directories: [iregex])
endif

if cdata.get('HAVE_FNMATCH')
    dep_fnmatch = dependency('', required: false)
else
    # use fnmatch bundled with ctags
    fnmatch = static_library('fnmatch',
        'ctags/fnmatch/fnmatch.c',
        'ctags/fnmatch/fnmatch.h',
        c_args: basic_cflags
    )
    dep_fnmatch = declare_dependency(link_with: [fnmatch], include_directories: [ifnmatch])
endif

ctags = static_library('ctags',
    'ctags/main/args.c',
    'ctags/main/args_p.h',
    'ctags/main/colprint.c',
    'ctags/main/colprint_p.h',
    'ctags/main/ctags.h',
    'ctags/main/debug.c',
    'ctags/main/debug.h',
    'ctags/main/dependency.c',
    'ctags/main/dependency.h',
    'ctags/main/dependency_p.h',
    'ctags/main/e_msoft.h',
    'ctags/main/entry.c',
    'ctags/main/entry.h',
    'ctags/main/entry_p.h',
    'ctags/main/entry_private.c',
    'ctags/main/error.c',
    'ctags/main/error_p.h',
    'ctags/main/field.c',
    'ctags/main/field.h',
    'ctags/main/field_p.h',
    'ctags/main/flags.c',
    'ctags/main/flags_p.h',
    'ctags/main/fmt.c',
    'ctags/main/fmt_p.h',
    'ctags/main/gcc-attr.h',
    'ctags/main/general.h',
    'ctags/main/gvars.h',
    'ctags/main/htable.c',
    'ctags/main/htable.h',
    'ctags/main/inline.h',
    'ctags/main/interactive_p.h',
    'ctags/main/keyword.c',
    'ctags/main/keyword.h',
    'ctags/main/keyword_p.h',
    'ctags/main/kind.c',
    'ctags/main/kind.h',
    'ctags/main/kind_p.h',
    'ctags/main/lregex.c',
    'ctags/main/lregex.h',
    'ctags/main/lregex_p.h',
    'ctags/main/lxpath.c',
    'ctags/main/lxpath.h',
    'ctags/main/lxpath_p.h',
    'ctags/main/main.c',
    'ctags/main/main_p.h',
    'ctags/main/mbcs.c',
    'ctags/main/mbcs.h',
    'ctags/main/mbcs_p.h',
    'ctags/main/mio.c',
    'ctags/main/mio.h',
    'ctags/main/nestlevel.c',
    'ctags/main/nestlevel.h',
    'ctags/main/numarray.c',
    'ctags/main/numarray.h',
    'ctags/main/objpool.c',
    'ctags/main/objpool.h',
    'ctags/main/options.c',
    'ctags/main/options.h',
    'ctags/main/options_p.h',
    'ctags/main/param.c',
    'ctags/main/param.h',
    'ctags/main/param_p.h',
    'ctags/main/parse.c',
    'ctags/main/parse.h',
    'ctags/main/parse_p.h',
    'ctags/main/parsers_p.h',
    'ctags/main/portable-dirent_p.h',
    'ctags/main/portable-scandir.c',
    'ctags/main/promise.c',
    'ctags/main/promise.h',
    'ctags/main/promise_p.h',
    'ctags/main/ptag.c',
    'ctags/main/ptag_p.h',
    'ctags/main/ptrarray.c',
    'ctags/main/ptrarray.h',
    'ctags/main/rbtree.c',
    'ctags/main/rbtree.h',
    'ctags/main/read.c',
    'ctags/main/read.h',
    'ctags/main/read_p.h',
    'ctags/main/repoinfo.c',
    'ctags/main/repoinfo.h',
    'ctags/main/routines.c',
    'ctags/main/routines.h',
    'ctags/main/routines_p.h',
    'ctags/main/seccomp.c',
    'ctags/main/selectors.c',
    'ctags/main/selectors.h',
    'ctags/main/sort.c',
    'ctags/main/sort_p.h',
    'ctags/main/stats.c',
    'ctags/main/stats_p.h',
    'ctags/main/strlist.c',
    'ctags/main/strlist.h',
    'ctags/main/subparser.h',
    'ctags/main/subparser_p.h',
    'ctags/main/tokeninfo.c',
    'ctags/main/tokeninfo.h',
    'ctags/main/trace.c',
    'ctags/main/trace.h',
    'ctags/main/trashbox.c',
    'ctags/main/trashbox.h',
    'ctags/main/trashbox_p.h',
    'ctags/main/types.h',
    'ctags/main/unwindi.c',
    'ctags/main/unwindi.h',
    'ctags/main/vstring.c',
    'ctags/main/vstring.h',
    'ctags/main/writer-ctags.c',
    'ctags/main/writer-etags.c',
    'ctags/main/writer-json.c',
    'ctags/main/writer-xref.c',
    'ctags/main/writer.c',
    'ctags/main/writer_p.h',
    'ctags/main/xtag.c',
    'ctags/main/xtag.h',
    'ctags/main/xtag_p.h',
    'ctags/parsers/geany_abaqus.c',
    'ctags/parsers/geany_abc.c',
    'ctags/parsers/geany_asciidoc.c',
    'ctags/parsers/geany_asm.c',
    'ctags/parsers/geany_basic.c',
    'ctags/parsers/geany_bibtex.c',
    'ctags/parsers/geany_c.c',
    'ctags/parsers/geany_cobol.c',
    'ctags/parsers/geany_iniconf.c',
    'ctags/parsers/geany_css.c',
    'ctags/parsers/geany_diff.c',
    'ctags/parsers/geany_docbook.c',
    'ctags/parsers/geany_erlang.c',
    'ctags/parsers/geany_flex.c',
    'ctags/parsers/geany_fortran.c',
    'ctags/parsers/geany_go.c',
    'ctags/parsers/geany_haskell.c',
    'ctags/parsers/geany_haxe.c',
    'ctags/parsers/geany_html.c',
    'ctags/parsers/geany_jscript.c',
    'ctags/parsers/geany_json.c',
    'ctags/parsers/geany_lcpp.c',
    'ctags/parsers/geany_lcpp.h',
    'ctags/parsers/geany_lua.c',
    'ctags/parsers/geany_make.c',
    'ctags/parsers/geany_markdown.c',
    'ctags/parsers/geany_matlab.c',
    'ctags/parsers/geany_nsis.c',
    'ctags/parsers/geany_objc.c',
    'ctags/parsers/geany_pascal.c',
    'ctags/parsers/geany_perl.c',
    'ctags/parsers/geany_php.c',
    'ctags/parsers/geany_powershell.c',
    'ctags/parsers/geany_python.c',
    'ctags/parsers/geany_r.c',
    'ctags/parsers/geany_rst.c',
    'ctags/parsers/geany_ruby.c',
    'ctags/parsers/geany_rust.c',
    'ctags/parsers/geany_sh.c',
    'ctags/parsers/geany_sql.c',
    'ctags/parsers/geany_tcl.c',
    'ctags/parsers/geany_tex.c',
    'ctags/parsers/geany_txt2tags.c',
    'ctags/parsers/geany_verilog.c',
    'ctags/parsers/geany_vhdl.c',
    c_args: geany_cflags + [ '-DEXTERNAL_PARSER_LIST_FILE="src/tagmanager/tm_parsers.h"' ],
    dependencies: deps + [dep_fnmatch],
    include_directories: [ictags]
)
dep_ctags = declare_dependency(link_with: [ctags], include_directories: [ictags])

install_headers(
    'src/tagmanager/tm_source_file.h',
    'src/tagmanager/tm_tag.h',
    'src/tagmanager/tm_workspace.h',
    'src/tagmanager/tm_parser.h',
    subdir: 'geany/tagmanager'
)
tagmanager = static_library('tagmanager',
    'src/tagmanager/tm_ctags.h',
    'src/tagmanager/tm_ctags.c',
    'src/tagmanager/tm_parser.h',
    'src/tagmanager/tm_parser.c',
    'src/tagmanager/tm_parsers.h',
    'src/tagmanager/tm_source_file.h',
    'src/tagmanager/tm_source_file.c',
    'src/tagmanager/tm_tag.h',
    'src/tagmanager/tm_tag.c',
    'src/tagmanager/tm_workspace.h',
    'src/tagmanager/tm_workspace.c',
    c_args: geany_cflags,
    dependencies: [dep_ctags, glib])
dep_tagmanager = declare_dependency(link_with: tagmanager, include_directories: [itagmanager])

# Generate signallist.i
gen_src = custom_target('gen-signallist',
                        input : [ 'scripts/gen-signallist.sh', 'data/geany.glade' ],
                        output : [ 'signallist.i' ],
                        command : [sh, '@INPUT0@', '@INPUT1@', '@OUTPUT@' ])

install_headers(
    'plugins/geanyfunctions.h',
    'plugins/geanyplugin.h',
    'src/app.h',
    'src/build.h',
    'src/dialogs.h',
    'src/document.h',
    'src/editor.h',
    'src/encodings.h',
    'src/filetypes.h',
    'src/geany.h',
    'src/gtkcompat.h',
    'src/highlighting.h',
    'src/keybindings.h',
    'src/main.h',
    'src/msgwindow.h',
    'src/navqueue.h',
    'src/plugindata.h',
    'src/pluginutils.h',
    'src/prefs.h',
    'src/project.h',
    'src/sciwrappers.h',
    'src/search.h',
    'src/spawn.h',
    'src/stash.h',
    'src/support.h',
    'src/symbols.h',
    'src/templates.h',
    'src/toolbar.h',
    'src/ui_utils.h',
    'src/utils.h',
    subdir: 'geany')

libgeany = shared_library('geany',
    'src/about.c',
    'src/about.h',
    'src/app.h',
    'src/build.c',
    'src/build.h',
    'src/callbacks.c',
    'src/callbacks.h',
    'src/dialogs.c',
    'src/dialogs.h',
    'src/document.c',
    'src/document.h',
    'src/editor.c',
    'src/editor.h',
    'src/encodings.c',
    'src/encodings.h',
    'src/filetypes.c',
    'src/filetypes.h',
    'src/geanyentryaction.c',
    'src/geanyentryaction.h',
    'src/geanymenubuttonaction.c',
    'src/geanymenubuttonaction.h',
    'src/geanyobject.c',
    'src/geanyobject.h',
    'src/geanywraplabel.c',
    'src/geanywraplabel.h',
    'src/gtkcompat.h',
    'src/highlighting.c',
    'src/highlighting.h',
    'src/highlightingmappings.h',
    'src/keybindings.c',
    'src/keybindings.h',
    'src/keyfile.c',
    'src/keyfile.h',
    'src/log.c',
    'src/log.h',
    'src/libmain.c',
    'src/main.h',
    'src/geany.h',
    'src/msgwindow.c',
    'src/msgwindow.h',
    'src/navqueue.c',
    'src/navqueue.h',
    'src/notebook.c',
    'src/notebook.h',
    'src/plugins.c',
    'src/plugins.h',
    'src/pluginutils.c',
    'src/pluginutils.h',
    'src/prefs.c',
    'src/prefs.h',
    'src/printing.c',
    'src/printing.h',
    'src/project.c',
    'src/project.h',
    'src/sciwrappers.c',
    'src/sciwrappers.h',
    'src/search.c',
    'src/search.h',
    'src/socket.c',
    'src/socket.h',
    'src/spawn.c',
    'src/spawn.h',
    'src/stash.c',
    'src/stash.h',
    'src/support.h',
    'src/symbols.c',
    'src/symbols.h',
    'src/templates.c',
    'src/templates.h',
    'src/toolbar.c',
    'src/toolbar.h',
    'src/tools.c',
    'src/tools.h',
    'src/sidebar.c',
    'src/sidebar.h',
    'src/ui_utils.c',
    'src/ui_utils.h',
    'src/utils.c',
    'src/utils.h',
    'src/vte.c',
    'src/vte.h',
    gen_src,
    soversion: '0',
    c_args: geany_cflags,
    include_directories: [iscintilla],
    dependencies: [dep_tagmanager, dep_ctags, dep_scintilla] + deps,
    install: true)
dep_libgeany = declare_dependency(link_with: libgeany, include_directories: [igeany])

executable('geany', 'src/main.c',
            link_with: libgeany,
            c_args: geany_cflags,
            dependencies: deps,
            build_rpath: meson.build_root(),
            install_rpath: '$ORIGIN/../' + get_option('libdir'),
            install: true)


i18n = import('i18n')

i18n.merge_file(
  input: 'geany.desktop.in',
  output: 'geany.desktop',
  type: 'desktop',
  po_dir: 'po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications')
)

subdir('po')
subdir('data')
subdir('doc')
subdir('icons')
subdir('plugins')
subdir('tests')

extra_data = [['COPYING', 'GPL-2']]
if host_machine.system() == 'windows'
extra_data += [['AUTHORS', 'Authors.txt']]
extra_data += [['ChangeLog', 'Changelog.txt']]
#extra_data += [['COPYING', 'Copying.txt']]
extra_data += [['README', 'Readme.txt']]
extra_data += [['NEWS', 'News.txt']]
extra_data += [['THANKS', 'Thanks.txt']]
extra_data += [['TODO', 'Todo.txt']]
endif
message(extra_data)
foreach file : extra_data
    custom_target('gen-'+ file[0].to_lower(),
                  input: file[0],
                  output: file[1],
                  command: [ 'cp', '@INPUT@', '@OUTPUT@' ],
                  install: true,
                  install_dir: join_paths(get_option('datadir'), 'geany'))
endforeach
geany_cflags += '-DGTK'
