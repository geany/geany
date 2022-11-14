project('geany', 'c', 'cpp',
        meson_version: '>= 0.53',
        version: '1.38',
        default_options : ['c_std=c11', 'cpp_std=c++17'])

gnome = import('gnome')
pymod = import('python')

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

dep_mac_integration = dependency('gtk-mac-integration', version: '>= 3.0.1',
	required: get_option('mac-integration'))

glib = deps[0]

# detect libc
glibc = false
if cc.has_header('gnu/libc-version.h')
	glibc = true
endif

gnu_source = glibc

cdata = configuration_data()
check_headers = [
	'alloca.h',
	'asprintf.h',
	'direct.h',
	'dirent.h',
	'dlfcn.h',
	'errno.h',
	'fcntl.h',
	'fnmatch.h',
	'inttypes.h',
	'io.h',
	'langinfo.h',
	'libintl.h',
	'limits.h',
	'locale.h',
	'memory.h',
	'stdbool.h',
	'sys/dir.h',
	'sys/stat.h',
	'sys/time.h',
	'sys/types.h',
	'unistd.h',
	'wchar.h',
	'wctype.h',
]

check_functions = [
	['alloca', '#include <alloca.h>'],
	['asprintf', '#include <stdio.h>'],
	['chsize', '#include <io.h>'],
	['fnmatch', '#include <fnmatch.h>'],
	['ftruncate', '#include <unistd.h>'],
	['isblank', '#include <ctype.h>'],
	['mbrtowc', '#include <wchar.h>'],
	['memcpy',  '#include <string.h>'],
	['mkstemp', '#include <stdlib.h>'],
	['realpath', '#include <limits.h>\n#include <stdlib.h>'],
	['regcomp', '#include <regex.h>'],
	['socket', '#include <sys/socket.h>'],
	# man page says strings.h but we include only string.h and it works
	['strcasecmp', '#include <string.h>'],
	['strncasecmp', '#include <string.h>'],
	['stricmp', '#include <string.h>'],
	['strnicmp', '#include <string.h>'],
	['strerror', '#include <string.h>'],
	['strstr', '#include <string.h>'],
	['tempnam', '#include <stdio.h>'],
	['truncate', '#include <unistd.h>'],
	['wcrtomb', '#include <wchar.h>'],
	['wcscoll', '#include <wchar.h>'],
	['g_strv_equal', '#include <glib.h>']
]

foreach h : check_headers
	define = 'HAVE_' + h.underscorify().to_upper()
	if cc.has_header(h)
		cdata.set(define, 1)
	else
		cdata.set(define, false)
	endif
endforeach

foreach f : check_functions
	define = 'HAVE_' + f.get(0).underscorify().to_upper()
	ccprefix = '\n'.join([gnu_source ? '#define _GNU_SOURCE' : '', f.get(1)])
	if cc.has_function(f.get(0), prefix: ccprefix, dependencies: deps)
		cdata.set(define, 1)
	else
		cdata.set(define, false)
	endif
endforeach

# From configure.ac:
# non-functions checks for u-ctags.  Not that we really need those as we don't
# use u-ctags's main, but the corresponding macros have to be defined to
# something, so simply perform the actual checks.
#AC_CHECK_DECLS([__environ],,,[[#include <unistd.h>]])
#AC_CHECK_DECLS([_NSGetEnviron],,,[[#include <crt_externs.h>]])
# In meson we cannot as easily test for declarations so defining as 0 is easiest
cdata.set('HAVE_DECL___ENVIRON', 0)
cdata.set('HAVE_DECL__NSGETENVIRON', 0)

cdata.set_quoted('PACKAGE', 'geany')
cdata.set_quoted('PACKAGE_BUGREPORT', 'https://github.com/geany/geany/issues')
cdata.set_quoted('PACKAGE_NAME','Geany')
cdata.set_quoted('PACKAGE_STRING', 'Geany ' + meson.project_version())
cdata.set_quoted('PACKAGE_VERSION', meson.project_version())
cdata.set('GETTEXT_PACKAGE', 'PACKAGE')
cdata.set('VERSION', meson.project_version())
cdata.set('ENABLE_NLS', 1)
if (host_machine.system() == 'windows')
	cdata.set('HAVE_VTE', false)
else
	cdata.set('HAVE_VTE', get_option('vte'))
endif
cdata.set('HAVE_PLUGINS', get_option('plugins'))
cdata.set('HAVE_SOCKET', get_option('socket'))
if (host_machine.system() == 'windows')
	cdata.set('GEANY_DATA_DIR', join_paths(prefix, 'data'))
	cdata.set('GEANY_DOC_DIR', join_paths(prefix))
else
	cdata.set('GEANY_DATA_DIR', join_paths(prefix, get_option('datadir'), 'geany'))
	cdata.set('GEANY_DOC_DIR', join_paths(prefix, get_option('datadir'), 'doc', 'geany'))
endif
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
doxygen = find_program('doxygen', required: get_option('api-docs'))
python = pymod.find_installation('python3', modules: doxygen.found() and get_option('gtkdoc') ? ['lxml'] : [])
# These two are truly optional
rst2html = find_program('rst2html', required: get_option('html-docs'))
rst2pdf = find_program('rst2pdf', required: get_option('pdf-docs'))
git = find_program('git', required: false)
if git.found()
	ret = run_command(git, 'rev-parse', '--short', '--revs-only', 'HEAD', check: true)
	cdata.set_quoted('REVISION', ret.stdout().strip())
else
	cdata.set_quoted('REVISION', '-1')
endif

python_command = get_option('python-command')
if python_command == '' or python_command == 'auto'
	if target_machine.system() == 'windows'
		python_command = 'py'
	else
		python_command = 'python'
	endif
endif
cdata.set('PYTHON_COMMAND', python_command)

configure_file(
	output : 'config.h',
	configuration : cdata
)

configure_file(
	input: 'geany.pc.in',
	install: true,
	install_dir: join_paths(prefix, get_option('libdir'), 'pkgconfig'),
	output: 'geany.pc',
	configuration: pcconf
)

# CFLAGS for basic stuff that only depends on libc
basic_cflags  = [ '-DHAVE_CONFIG_H=1', '-O2' ]
if (gnu_source)
	basic_cflags += '-D_GNU_SOURCE'
endif

# CFLAGS common between Geany and bundled plugins
def_cflags = basic_cflags + [
	'-DGTK',
# we're using lots of deprecated stuff with gtk3 (often because there is no proper alternative!)
	'-Wno-deprecated-declarations',
	'-DGDK_DISABLE_DEPRECATION_WARNINGS',
	'-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_32',
	'-DGEANY_PREFIX="@0@"'.format(get_option('prefix')),
	'-DGEANY_DOCDIR="@0@"'.format(join_paths(prefix, get_option('datadir'), 'doc'))
]
foreach d : [ 'includedir', 'libdir', 'libexecdir', 'datadir', 'localedir' ]
	def_cflags += '-DGEANY_@0@="@1@"'.format(d.underscorify().to_upper(), join_paths(prefix, get_option(d)))
endforeach

# CFLAGS for everything else, i.e. most of Geany
geany_cflags = def_cflags
have_gcc4_visibility = cc.has_argument('-fvisibility=hidden')
geany_cflags += '-DGEANY_PRIVATE'
if target_machine.system() == 'windows'
	geany_cflags += '-DGEANY_EXPORT_SYMBOL=__declspec(dllexport)'
elif have_gcc4_visibility
	geany_cflags += '-fvisibility=hidden'
	geany_cflags += '-DGEANY_EXPORT_SYMBOL=__attribute__((visibility("default")))'
endif
geany_cflags += '-DGEANY_API_SYMBOL=GEANY_EXPORT_SYMBOL'
sci_cflags = geany_cflags
sci_cflags += [ '-std=c++17', '-Wno-non-virtual-dtor', '-DNDEBUG', '-DSCI_LEXER', '-DNO_CXX11_REGEX' ]

iregex = include_directories('ctags/gnu_regex')
ifnmatch = include_directories('ctags/fnmatch')
ictags = include_directories('ctags/main', 'ctags/parsers', 'ctags/dsl')
itagmanager = include_directories('src/tagmanager')
iscintilla = include_directories('scintilla/include', 'scintilla/lexilla/include')
igeany = include_directories('src')

install_headers(
	'scintilla/include/Scintilla.h',
	'scintilla/include/Scintilla.iface',
	'scintilla/include/ScintillaWidget.h',
	'scintilla/include/Sci_Position.h',
	'scintilla/lexilla/include/SciLexer.h',
	subdir: 'geany/scintilla'
)

lexilla = static_library('lexilla',
	'scintilla/lexilla/include/Lexilla.h',
	'scintilla/lexilla/include/SciLexer.h',
	'scintilla/lexilla/lexers/LexAbaqus.cxx',
	'scintilla/lexilla/lexers/LexAda.cxx',
	'scintilla/lexilla/lexers/LexAsciidoc.cxx',
	'scintilla/lexilla/lexers/LexAsm.cxx',
	'scintilla/lexilla/lexers/LexBash.cxx',
	'scintilla/lexilla/lexers/LexBasic.cxx',
	'scintilla/lexilla/lexers/LexBatch.cxx',
	'scintilla/lexilla/lexers/LexCaml.cxx',
	'scintilla/lexilla/lexers/LexCmake.cxx',
	'scintilla/lexilla/lexers/LexCOBOL.cxx',
	'scintilla/lexilla/lexers/LexCoffeeScript.cxx',
	'scintilla/lexilla/lexers/LexCPP.cxx',
	'scintilla/lexilla/lexers/LexCSS.cxx',
	'scintilla/lexilla/lexers/LexD.cxx',
	'scintilla/lexilla/lexers/LexDiff.cxx',
	'scintilla/lexilla/lexers/LexErlang.cxx',
	'scintilla/lexilla/lexers/LexForth.cxx',
	'scintilla/lexilla/lexers/LexGDScript.cxx',
	'scintilla/lexilla/lexers/LexFortran.cxx',
	'scintilla/lexilla/lexers/LexHaskell.cxx',
	'scintilla/lexilla/lexers/LexHTML.cxx',
	'scintilla/lexilla/lexers/LexJulia.cxx',
	'scintilla/lexilla/lexers/LexLaTeX.cxx',
	'scintilla/lexilla/lexers/LexLisp.cxx',
	'scintilla/lexilla/lexers/LexLua.cxx',
	'scintilla/lexilla/lexers/LexMake.cxx',
	'scintilla/lexilla/lexers/LexMarkdown.cxx',
	'scintilla/lexilla/lexers/LexMatlab.cxx',
	'scintilla/lexilla/lexers/LexNsis.cxx',
	'scintilla/lexilla/lexers/LexNull.cxx',
	'scintilla/lexilla/lexers/LexPascal.cxx',
	'scintilla/lexilla/lexers/LexPerl.cxx',
	'scintilla/lexilla/lexers/LexPO.cxx',
	'scintilla/lexilla/lexers/LexPowerShell.cxx',
	'scintilla/lexilla/lexers/LexProps.cxx',
	'scintilla/lexilla/lexers/LexPython.cxx',
	'scintilla/lexilla/lexers/LexR.cxx',
	'scintilla/lexilla/lexers/LexRuby.cxx',
	'scintilla/lexilla/lexers/LexRust.cxx',
	'scintilla/lexilla/lexers/LexSmalltalk.cxx',
	'scintilla/lexilla/lexers/LexSQL.cxx',
	'scintilla/lexilla/lexers/LexTCL.cxx',
	'scintilla/lexilla/lexers/LexTxt2tags.cxx',
	'scintilla/lexilla/lexers/LexVerilog.cxx',
	'scintilla/lexilla/lexers/LexVHDL.cxx',
	'scintilla/lexilla/lexers/LexYAML.cxx',
	'scintilla/lexilla/lexlib/Accessor.cxx',
	'scintilla/lexilla/lexlib/Accessor.h',
	'scintilla/lexilla/lexlib/CatalogueModules.h',
	'scintilla/lexilla/lexlib/CharacterCategory.cxx',
	'scintilla/lexilla/lexlib/CharacterCategory.h',
	'scintilla/lexilla/lexlib/CharacterSet.cxx',
	'scintilla/lexilla/lexlib/CharacterSet.h',
	'scintilla/lexilla/lexlib/DefaultLexer.cxx',
	'scintilla/lexilla/lexlib/DefaultLexer.h',
	'scintilla/lexilla/lexlib/LexAccessor.cxx',
	'scintilla/lexilla/lexlib/LexAccessor.h',
	'scintilla/lexilla/lexlib/LexerBase.cxx',
	'scintilla/lexilla/lexlib/LexerBase.h',
	'scintilla/lexilla/lexlib/LexerModule.cxx',
	'scintilla/lexilla/lexlib/LexerModule.h',
	'scintilla/lexilla/lexlib/LexerNoExceptions.cxx',
	'scintilla/lexilla/lexlib/LexerNoExceptions.h',
	'scintilla/lexilla/lexlib/LexerSimple.cxx',
	'scintilla/lexilla/lexlib/LexerSimple.h',
	'scintilla/lexilla/lexlib/OptionSet.h',
	'scintilla/lexilla/lexlib/PropSetSimple.cxx',
	'scintilla/lexilla/lexlib/PropSetSimple.h',
	'scintilla/lexilla/lexlib/SparseState.h',
	'scintilla/lexilla/lexlib/StringCopy.h',
	'scintilla/lexilla/lexlib/StyleContext.cxx',
	'scintilla/lexilla/lexlib/StyleContext.h',
	'scintilla/lexilla/lexlib/SubStyles.h',
	'scintilla/lexilla/lexlib/WordList.cxx',
	'scintilla/lexilla/lexlib/WordList.h',
	'scintilla/lexilla/src/Lexilla.cxx',
	cpp_args: sci_cflags,
	dependencies: deps,
	include_directories: [
		iscintilla,
		include_directories('scintilla/lexilla/lexlib', 'scintilla/lexilla/include')
	]
)
dep_lexilla = declare_dependency(
	link_with: lexilla,
	include_directories: include_directories('scintilla/lexilla/include')
)

scintilla = static_library('scintilla',
	'scintilla/gtk/Converter.h',
	'scintilla/gtk/PlatGTK.cxx',
	'scintilla/gtk/ScintillaGTKAccessible.cxx',
	'scintilla/gtk/ScintillaGTKAccessible.h',
	'scintilla/gtk/ScintillaGTK.cxx',
	'scintilla/gtk/ScintillaGTK.h',
	'scintilla/gtk/scintilla-marshal.c',
	'scintilla/gtk/scintilla-marshal.h',
	'scintilla/include/ILexer.h',
	'scintilla/include/ILoader.h',
	'scintilla/include/ScintillaCall.h',
	'scintilla/include/Scintilla.h',
	'scintilla/include/ScintillaMessages.h',
	'scintilla/include/ScintillaStructures.h',
	'scintilla/include/ScintillaTypes.h',
	'scintilla/include/ScintillaWidget.h',
	'scintilla/include/Sci_Position.h',
	'scintilla/src/AutoComplete.cxx',
	'scintilla/src/AutoComplete.h',
	'scintilla/src/CallTip.cxx',
	'scintilla/src/CallTip.h',
	'scintilla/src/CaseConvert.cxx',
	'scintilla/src/CaseConvert.h',
	'scintilla/src/CaseFolder.cxx',
	'scintilla/src/CaseFolder.h',
	'scintilla/src/CellBuffer.cxx',
	'scintilla/src/CellBuffer.h',
	'scintilla/src/CharacterCategoryMap.cxx',
	'scintilla/src/CharacterCategoryMap.h',
	'scintilla/src/CharacterType.cxx',
	'scintilla/src/CharacterType.h',
	'scintilla/src/CharClassify.cxx',
	'scintilla/src/CharClassify.h',
	'scintilla/src/ContractionState.cxx',
	'scintilla/src/ContractionState.h',
	'scintilla/src/DBCS.cxx',
	'scintilla/src/DBCS.h',
	'scintilla/src/Debugging.h',
	'scintilla/src/Decoration.cxx',
	'scintilla/src/Decoration.h',
	'scintilla/src/Document.cxx',
	'scintilla/src/Document.h',
	'scintilla/src/EditModel.cxx',
	'scintilla/src/EditModel.h',
	'scintilla/src/Editor.cxx',
	'scintilla/src/Editor.h',
	'scintilla/src/EditView.cxx',
	'scintilla/src/EditView.h',
	'scintilla/src/ElapsedPeriod.h',
	'scintilla/src/FontQuality.h',
	'scintilla/src/Geometry.cxx',
	'scintilla/src/Geometry.h',
	'scintilla/src/Indicator.cxx',
	'scintilla/src/Indicator.h',
	'scintilla/src/KeyMap.cxx',
	'scintilla/src/KeyMap.h',
	'scintilla/src/LineMarker.cxx',
	'scintilla/src/LineMarker.h',
	'scintilla/src/MarginView.cxx',
	'scintilla/src/MarginView.h',
	'scintilla/src/Partitioning.h',
	'scintilla/src/PerLine.cxx',
	'scintilla/src/PerLine.h',
	'scintilla/src/Platform.h',
	'scintilla/src/PositionCache.cxx',
	'scintilla/src/PositionCache.h',
	'scintilla/src/Position.h',
	'scintilla/src/RESearch.cxx',
	'scintilla/src/RESearch.h',
	'scintilla/src/RunStyles.cxx',
	'scintilla/src/RunStyles.h',
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
	'scintilla/src/UniqueString.cxx',
	'scintilla/src/UniqueString.h',
	'scintilla/src/ViewStyle.cxx',
	'scintilla/src/ViewStyle.h',
	'scintilla/src/XPM.cxx',
	'scintilla/src/XPM.h',
	cpp_args: sci_cflags,
	dependencies: deps + [ dep_lexilla ],
	include_directories: [
		iscintilla,
		include_directories('scintilla/include', 'scintilla/src')
	]
)
dep_scintilla = declare_dependency(
	link_with: scintilla,
	include_directories: include_directories('scintilla/include')
)

if cdata.get('HAVE_FNMATCH') == 1
	dep_fnmatch = dependency('', required: false)
else
	# use fnmatch bundled with ctags
	fnmatch = static_library('fnmatch',
		'ctags/fnmatch/fnmatch.c',
		'ctags/fnmatch/fnmatch.h',
		include_directories: [ifnmatch],
		c_args: basic_cflags
	)
	dep_fnmatch = declare_dependency(link_with: [fnmatch], include_directories: [ifnmatch])
endif

if cdata.get('HAVE_REGCOMP') == 1
	dep_regex = dependency('', required: false)
else
	# use regcomp bundled with ctags
	regex = static_library('regex',
		'ctags/gnu_regex/regex.c',
		'ctags/gnu_regex/regex.h',
		dependencies: [dep_fnmatch],
		c_args: basic_cflags + [ '-D__USE_GNU' ]
	)
	dep_regex = declare_dependency(link_with: [regex], include_directories: [iregex])
endif

ctags = static_library('ctags',
	'ctags/dsl/es.c',
	'ctags/dsl/es.h',
	'ctags/dsl/optscript.c',
	'ctags/dsl/optscript.h',
	'ctags/main/args.c',
	'ctags/main/args_p.h',
	'ctags/main/colprint.c',
	'ctags/main/colprint_p.h',
	'ctags/main/CommonPrelude.c',
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
	'ctags/main/lregex-default.c',
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
	'ctags/main/script.c',
	'ctags/main/script_p.h',
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
	'ctags/main/writer.c',
	'ctags/main/writer-ctags.c',
	'ctags/main/writer-etags.c',
	'ctags/main/writer-json.c',
	'ctags/main/writer_p.h',
	'ctags/main/writer-xref.c',
	'ctags/main/xtag.c',
	'ctags/main/xtag.h',
	'ctags/main/xtag_p.h',
	'ctags/parsers/abaqus.c',
	'ctags/parsers/abc.c',
	'ctags/parsers/ada.c',
	'ctags/parsers/asciidoc.c',
	'ctags/parsers/asm.c',
	'ctags/parsers/basic.c',
	'ctags/parsers/bibtex.c',
	'ctags/parsers/clojure.c',
	'ctags/parsers/cobol.c',
	'ctags/parsers/cpreprocessor.c',
	'ctags/parsers/cpreprocessor.h',
	'ctags/parsers/css.c',
	'ctags/parsers/cxx/cxx.c',
	'ctags/parsers/cxx/cxx_debug.c',
	'ctags/parsers/cxx/cxx_debug.h',
	'ctags/parsers/cxx/cxx_debug_type.c',
	'ctags/parsers/cxx/cxx_keyword.c',
	'ctags/parsers/cxx/cxx_keyword.h',
	'ctags/parsers/cxx/cxx_parser_block.c',
	'ctags/parsers/cxx/cxx_parser.c',
	'ctags/parsers/cxx/cxx_parser_function.c',
	'ctags/parsers/cxx/cxx_parser.h',
	'ctags/parsers/cxx/cxx_parser_internal.h',
	'ctags/parsers/cxx/cxx_parser_lambda.c',
	'ctags/parsers/cxx/cxx_parser_namespace.c',
	'ctags/parsers/cxx/cxx_parser_template.c',
	'ctags/parsers/cxx/cxx_parser_tokenizer.c',
	'ctags/parsers/cxx/cxx_parser_typedef.c',
	'ctags/parsers/cxx/cxx_parser_using.c',
	'ctags/parsers/cxx/cxx_parser_variable.c',
	'ctags/parsers/cxx/cxx_qtmoc.c',
	'ctags/parsers/cxx/cxx_scope.c',
	'ctags/parsers/cxx/cxx_scope.h',
	'ctags/parsers/cxx/cxx_subparser.c',
	'ctags/parsers/cxx/cxx_subparser.h',
	'ctags/parsers/cxx/cxx_subparser_internal.h',
	'ctags/parsers/cxx/cxx_tag.c',
	'ctags/parsers/cxx/cxx_tag.h',
	'ctags/parsers/cxx/cxx_token.c',
	'ctags/parsers/cxx/cxx_token_chain.c',
	'ctags/parsers/cxx/cxx_token_chain.h',
	'ctags/parsers/cxx/cxx_token.h',
	'ctags/parsers/diff.c',
	'ctags/parsers/erlang.c',
	'ctags/parsers/flex.c',
	'ctags/parsers/fortran.c',
	'ctags/parsers/gdscript.c',
	'ctags/parsers/geany_c.c',
	'ctags/parsers/geany_docbook.c',
	'ctags/parsers/geany_lcpp.c',
	'ctags/parsers/geany_lcpp.h',
	'ctags/parsers/geany_matlab.c',
	'ctags/parsers/go.c',
	'ctags/parsers/haskell.c',
	'ctags/parsers/haxe.c',
	'ctags/parsers/html.c',
	'ctags/parsers/iniconf.c',
	'ctags/parsers/iniconf.h',
	'ctags/parsers/jscript.c',
	'ctags/parsers/json.c',
	'ctags/parsers/julia.c',
	'ctags/parsers/lisp.c',
	'ctags/parsers/lua.c',
	'ctags/parsers/make.c',
	'ctags/parsers/make.h',
	'ctags/parsers/markdown.c',
	'ctags/parsers/markdown.h',
	'ctags/parsers/nsis.c',
	'ctags/parsers/objc.c',
	'ctags/parsers/pascal.c',
	'ctags/parsers/perl.c',
	'ctags/parsers/perl.h',
	'ctags/parsers/php.c',
	'ctags/parsers/powershell.c',
	'ctags/parsers/python.c',
	'ctags/parsers/r.c',
	'ctags/parsers/r.h',
	'ctags/parsers/rst.c',
	'ctags/parsers/ruby.c',
	'ctags/parsers/rust.c',
	'ctags/parsers/sh.c',
	'ctags/parsers/sql.c',
	'ctags/parsers/tcl.c',
	'ctags/parsers/tcl.h',
	'ctags/parsers/tcloo.c',
	'ctags/parsers/tex.c',
	'ctags/parsers/tex.h',
	'ctags/parsers/txt2tags.c',
	'ctags/parsers/typescript.c',
	'ctags/parsers/verilog.c',
	'ctags/parsers/vhdl.c',
	c_args: geany_cflags + [ '-DG_LOG_DOMAIN="CTags"',
	                         '-DEXTERNAL_PARSER_LIST_FILE="src/tagmanager/tm_parsers.h"' ],
	dependencies: deps + [dep_fnmatch, dep_regex],
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
	c_args: geany_cflags + [ '-DG_LOG_DOMAIN="Tagmanager"' ],
	dependencies: [dep_ctags, glib]
)
dep_tagmanager = declare_dependency(
	link_with: tagmanager,
	include_directories: [itagmanager]
)

# Generate signallist.i
gen_src = custom_target('gen-signallist',
	input : [ 'data/geany.glade' ],
	output : [ 'signallist.i' ],
	command : [find_program('scripts/gen-signallist.sh'), '@INPUT@', '@OUTPUT@' ]
)

win_src = []
win_deps = []
if (host_machine.system() == 'windows')
	win_src += [ 'src/win32.c', 'src/win32.h' ]
	foreach lib : ['ole32', 'wsock32', 'comdlg32']
		win_deps += cc.find_library(lib)
	endforeach
endif

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
	subdir: 'geany'
)

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
	gen_src,
	win_src,
	dep_mac_integration.found() ? ['src/osx.c', 'src/osx.h'] : [],
	host_machine.system() == 'windows' ? ['src/win32.c',  'src/win32.h'] : [ 'src/vte.c', 'src/vte.h' ],
	soversion: '0',
	c_args: geany_cflags + [ '-DG_LOG_DOMAIN="Geany"' ],
	include_directories: [iscintilla],
	dependencies: [dep_tagmanager, dep_ctags, dep_scintilla, dep_mac_integration] + deps + win_deps,
	install: true
)
dep_libgeany = declare_dependency(
	link_with: libgeany,
	include_directories: [iscintilla, itagmanager, igeany]
)

executable('geany',
	'src/main.c',
	link_with: libgeany,
	c_args: geany_cflags + [ '-DG_LOG_DOMAIN="Geany"' ],
	dependencies: deps,
	build_rpath: meson.build_root(),
	install_rpath: '$ORIGIN/../' + get_option('libdir'),
	install: true
)

i18n = import('i18n')

desktop_file = 'geany.desktop'
desktop_output_file = i18n.merge_file(
	type: 'desktop',
	input: desktop_file + '.in',
	output: desktop_file,
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

install_data('COPYING', rename: 'GPL-2')
install_data(
	'scintilla/License.txt',
	'scintilla/lexilla/License.txt',
	rename: [
		'ScintillaLicense.txt',
		'LexillaLicense.txt'
	],
	install_dir: cdata.get('GEANY_DOC_DIR')
)
misc = [
	'AUTHORS',
	'COPYING',
	'ChangeLog',
	'NEWS',
	'README',
	'THANKS',
	'TODO'
]
install_data(misc, install_dir: cdata.get('GEANY_DOC_DIR'))
if host_machine.system() == 'windows'
misc_rename = [
	'Authors.txt',
	'Changelog.txt',
	'Copying.txt',
	'Readme.txt',
	'News.txt',
	'Thanks.txt',
	'Todo.txt'
]
install_data(misc, install_dir: prefix, rename: misc_rename)
endif
