#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# WAF build script - this file is part of Geany, a fast and lightweight IDE
#
# Copyright 2008-2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
# Copyright 2008-2010 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# $Id$

"""
This is a WAF build script (http://code.google.com/p/waf/).
It can be used as an alternative build system to autotools
for Geany. It does not (yet) cover all of the autotools tests and
configure options but all important things are working.
"make dist" should be done with autotools, most other targets and
functions should work better (regarding performance and flexibility)
or at least equally.

Missing features: --enable-binreloc, make targets: dist, pdf (in doc/)
Known issues: Dependency handling is buggy, e.g. if src/document.h is
			  changed, depending source files are not rebuilt (maybe Waf bug).

The code of this file itself loosely follows PEP 8 with some exceptions
(line width 100 characters and some other minor things).

Requires WAF 1.5.7 and Python 2.4 (or later).
"""


import Build
import Configure
import Options
import Utils
import sys
import os
import shutil
import tempfile
from distutils import version
from TaskGen import taskgen, feature


APPNAME = 'geany'
VERSION = '0.20'

srcdir = '.'
blddir = '_build_'


tagmanager_sources = [
	'tagmanager/args.c', 'tagmanager/abc.c', 'tagmanager/actionscript.c', 'tagmanager/asm.c',
	'tagmanager/basic.c', 'tagmanager/c.c',
	'tagmanager/conf.c', 'tagmanager/css.c', 'tagmanager/ctags.c', 'tagmanager/diff.c',
	'tagmanager/docbook.c', 'tagmanager/entry.c', 'tagmanager/fortran.c', 'tagmanager/get.c',
	'tagmanager/haskell.c', 'tagmanager/haxe.c', 'tagmanager/html.c', 'tagmanager/js.c',
	'tagmanager/keyword.c', 'tagmanager/latex.c', 'tagmanager/lregex.c', 'tagmanager/lua.c',
	'tagmanager/make.c', 'tagmanager/markdown.c', 'tagmanager/matlab.c', 'tagmanager/nsis.c',
	'tagmanager/nestlevel.c', 'tagmanager/options.c',
	'tagmanager/parse.c', 'tagmanager/pascal.c', 'tagmanager/r.c',
	'tagmanager/perl.c', 'tagmanager/php.c', 'tagmanager/python.c', 'tagmanager/read.c',
	'tagmanager/rest.c', 'tagmanager/ruby.c', 'tagmanager/sh.c', 'tagmanager/sort.c',
	'tagmanager/sql.c', 'tagmanager/strlist.c', 'tagmanager/txt2tags.c', 'tagmanager/tcl.c',
	'tagmanager/tm_file_entry.c',
	'tagmanager/tm_project.c', 'tagmanager/tm_source_file.c', 'tagmanager/tm_symbol.c',
	'tagmanager/tm_tag.c', 'tagmanager/tm_tagmanager.c', 'tagmanager/tm_work_object.c',
	'tagmanager/tm_workspace.c', 'tagmanager/vhdl.c', 'tagmanager/verilog.c', 'tagmanager/vstring.c' ]

scintilla_sources = [
	'scintilla/AutoComplete.cxx', 'scintilla/CallTip.cxx', 'scintilla/CellBuffer.cxx',
	'scintilla/CharClassify.cxx', 'scintilla/ContractionState.cxx', 'scintilla/Decoration.cxx',
	'scintilla/DocumentAccessor.cxx', 'scintilla/Document.cxx', 'scintilla/Editor.cxx',
	'scintilla/ExternalLexer.cxx',  'scintilla/Indicator.cxx',  'scintilla/KeyMap.cxx',
	'scintilla/KeyWords.cxx',
	'scintilla/LexAda.cxx', 'scintilla/LexAsm.cxx', 'scintilla/LexBash.cxx',
	'scintilla/LexBasic.cxx', 'scintilla/LexCaml.cxx', 'scintilla/LexCmake.cxx', 'scintilla/LexCPP.cxx',
	'scintilla/LexCSS.cxx', 'scintilla/LexD.cxx', 'scintilla/LexForth.cxx',
	'scintilla/LexFortran.cxx', 'scintilla/LexHaskell.cxx', 'scintilla/LexHTML.cxx',
	'scintilla/LexLua.cxx', 'scintilla/LexMarkdown.cxx', 'scintilla/LexMatlab.cxx',
	'scintilla/LexNsis.cxx', 'scintilla/LexOthers.cxx',
	'scintilla/LexPascal.cxx', 'scintilla/LexPerl.cxx', 'scintilla/LexPython.cxx',
	'scintilla/LexR.cxx', 'scintilla/LexRuby.cxx', 'scintilla/LexSQL.cxx',
	'scintilla/LexTCL.cxx', 'scintilla/LexTxt2tags.cxx',
	'scintilla/LexVHDL.cxx', 'scintilla/LexVerilog.cxx', 'scintilla/LexYAML.cxx',
	'scintilla/LineMarker.cxx', 'scintilla/PerLine.cxx',
	'scintilla/PlatGTK.cxx',
	'scintilla/PositionCache.cxx', 'scintilla/PropSet.cxx', 'scintilla/RESearch.cxx',
	'scintilla/RunStyles.cxx', 'scintilla/ScintillaBase.cxx', 'scintilla/ScintillaGTK.cxx',
	'scintilla/scintilla-marshal.c', 'scintilla/Selection.cxx', 'scintilla/StyleContext.cxx', 'scintilla/Style.cxx',
	'scintilla/UniConversion.cxx', 'scintilla/ViewStyle.cxx', 'scintilla/WindowAccessor.cxx',
	'scintilla/XPM.cxx' ]

geany_sources = [
	'src/about.c', 'src/build.c', 'src/callbacks.c', 'src/dialogs.c', 'src/document.c',
	'src/editor.c', 'src/encodings.c', 'src/filetypes.c', 'src/geanyentryaction.c',
	'src/geanymenubuttonaction.c', 'src/geanyobject.c', 'src/geanywraplabel.c',
	'src/highlighting.c', 'src/interface.c', 'src/keybindings.c',
	'src/keyfile.c', 'src/log.c', 'src/main.c', 'src/msgwindow.c', 'src/navqueue.c', 'src/notebook.c',
	'src/plugins.c', 'src/pluginutils.c', 'src/prefix.c', 'src/prefs.c', 'src/printing.c', 'src/project.c',
	'src/sciwrappers.c', 'src/search.c', 'src/socket.c', 'src/stash.c',
	'src/symbols.c',
	'src/templates.c', 'src/toolbar.c', 'src/tools.c', 'src/sidebar.c',
	'src/ui_utils.c', 'src/utils.c' ]



def configure(conf):
	def in_git():
		cmd = 'git ls-files >/dev/null 2>&1'
		return (Utils.exec_command(cmd) == 0)

	def in_svn():
		return os.path.exists('.svn')

	def conf_get_svn_rev():
		# try GIT
		if in_git():
			cmds = [ 'git svn find-rev HEAD 2>/dev/null',
					 'git svn find-rev origin/trunk 2>/dev/null',
					 'git svn find-rev trunk 2>/dev/null',
					 'git svn find-rev master 2>/dev/null'
					]
			for c in cmds:
				try:
					stdout = Utils.cmd_output(c)
					if stdout:
						return stdout.strip()
				except:
					pass
		# try SVN
		elif in_svn():
			try:
				_env = None if is_win32 else {'LANG' : 'C'}
				stdout = Utils.cmd_output(cmd='svn info --non-interactive', silent=True, env=_env)
				lines = stdout.splitlines(True)
				for line in lines:
					if line.startswith('Last Changed Rev'):
						key, value = line.split(': ', 1)
						return value.strip()
			except:
				pass
		else:
			pass
		return '-1'

	def conf_define_from_opt(define_name, opt_name, default_value, quote=1):
		if opt_name:
			if isinstance(opt_name, bool):
				opt_name = 1
			conf.define(define_name, opt_name, quote)
		elif default_value:
			conf.define(define_name, default_value, quote)


	conf.check_tool('compiler_cc')
	is_win32 = target_is_win32(conf.env)

	conf.check(header_name='fcntl.h')
	conf.check(header_name='fnmatch.h')
	conf.check(header_name='glob.h')
	conf.check(header_name='sys/time.h')
	conf.check(header_name='sys/types.h')
	conf.check(header_name='sys/stat.h')
	conf.define('HAVE_STDLIB_H', 1) # are there systems without stdlib.h?
	conf.define('STDC_HEADERS', 1) # an optimistic guess ;-)

	if Options.options.gnu_regex:
		conf.define('HAVE_REGCOMP', 1, 0)
		conf.define('USE_INCLUDED_REGEX', 1, 0)
	else:
		conf.check(header_name='regex.h')
		if conf.env['HAVE_REGEX_H'] == 1:
			conf.check(function_name='regcomp', header_name='regex.h')
		# fallback to included regex lib
		if conf.env['HAVE_REGCOMP'] != 1 or conf.env['HAVE_REGEX_H'] != 1:
			conf.define('HAVE_REGCOMP', 1, 0)
			conf.define('USE_INCLUDED_REGEX', 1, 0)
			Utils.pprint('YELLOW', 'Using included GNU regex library.')

	conf.check(function_name='fgetpos', header_name='stdio.h')
	conf.check(function_name='ftruncate', header_name='unistd.h')
	conf.check(function_name='gethostname', header_name='unistd.h')
	conf.check(function_name='mkstemp', header_name='stdlib.h')
	conf.check(function_name='strstr', header_name='string.h', mandatory=True)

	# check sunOS socket support
	if Options.platform == 'sunos':
		conf.check(function_name='socket', lib='socket',
			header_name='sys/socket.h', uselib_store='SUNOS_SOCKET', mandatory=True)

	# check for cxx after the header and function checks have been done to ensure they are
	# checked with cc not cxx
	conf.check_tool('compiler_cxx misc')
	if is_win32:
		conf.check_tool('winres')
	# we don't require intltool on Windows (it would require Perl) though it works well
	try:
		conf.check_tool('intltool')
		if 'LINGUAS' in os.environ:
			conf.env['LINGUAS'] = os.environ['LINGUAS']
	except Configure.ConfigurationError:
		if not is_win32:
			raise

	# GTK / GIO version check
	conf.check_cfg(package='gtk+-2.0', atleast_version='2.8.0', uselib_store='GTK',
		mandatory=True, args='--cflags --libs')
	have_gtk_210 = False
	gtk_version = conf.check_cfg(modversion='gtk+-2.0', uselib_store='GTK')
	if gtk_version:
		if version.LooseVersion(gtk_version) >= version.LooseVersion('2.10.0'):
			have_gtk_210 = True
	else:
		gtk_version = 'Unknown'
	conf.check_cfg(package='gio-2.0', uselib_store='GIO', args='--cflags --libs', mandatory=False)
	conf.check_cfg(package='x11', uselib_store='X11', args='--cflags --libs', mandatory=False)

	# Windows specials
	if is_win32:
		if conf.env['PREFIX'] == tempfile.gettempdir():
			# overwrite default prefix on Windows (tempfile.gettempdir() is the Waf default)
			conf.define('PREFIX', os.path.join(conf.srcdir, '%s-%s' % (APPNAME, VERSION)), 1)
		conf.define('DOCDIR', os.path.join(conf.env['PREFIX'], 'doc'), 1)
		conf.define('LOCALEDIR', os.path.join(conf.env['PREFIX'], 'share\locale'), 1)
		conf.define('LIBDIR', conf.env['PREFIX'], 1)
		# DATADIR is defined in objidl.h, so we remove it from config.h but keep it in env
		conf.undefine('DATADIR')
		conf.env['DATADIR'] = os.path.join(conf.env['PREFIX'], 'data')
		# hack: we add the parent directory of the first include directory as this is missing in
		# list returned from pkg-config
		conf.env['CPPPATH_GTK'].insert(0, os.path.dirname(conf.env['CPPPATH_GTK'][0]))
		# we don't need -fPIC when compiling on or for Windows
		if '-fPIC' in conf.env['shlib_CCFLAGS']:
			conf.env['shlib_CCFLAGS'].remove('-fPIC')
		if '-fPIC' in conf.env['CXXFLAGS']:
			conf.env['CXXFLAGS'].remove('-fPIC')
		conf.env.append_value('program_LINKFLAGS', '-mwindows')
		conf.env.append_value('LIB_WIN32', [ 'wsock32', 'uuid', 'ole32', 'iberty' ])
		conf.env['shlib_PATTERN'] = '%s.dll'
		conf.env['program_PATTERN'] = '%s.exe'
	else:
		conf.env['shlib_PATTERN'] = '%s.so'
		conf_define_from_opt('DOCDIR', Options.options.docdir,
			os.path.join(conf.env['DATADIR'], 'doc/geany'))
		conf_define_from_opt('LIBDIR', Options.options.libdir,
			os.path.join(conf.env['PREFIX'], 'lib'))
		conf_define_from_opt('MANDIR', Options.options.mandir,
			os.path.join(conf.env['DATADIR'], 'man'))

	svn_rev = conf_get_svn_rev()
	conf.define('ENABLE_NLS', 1)
	conf.define('GEANY_LOCALEDIR', '' if is_win32 else conf.env['LOCALEDIR'], 1)
	conf.define('GEANY_DATADIR', 'data' if is_win32 else conf.env['DATADIR'], 1)
	conf.define('GEANY_DOCDIR', conf.env['DOCDIR'], 1)
	conf.define('GEANY_LIBDIR', '' if is_win32 else conf.env['LIBDIR'], 1)
	conf.define('GEANY_PREFIX', '' if is_win32 else conf.env['PREFIX'], 1)
	conf.define('PACKAGE', APPNAME, 1)
	conf.define('VERSION', VERSION, 1)
	conf.define('REVISION', svn_rev, 1)

	conf.define('GETTEXT_PACKAGE', APPNAME, 1)

	conf_define_from_opt('HAVE_PLUGINS', not Options.options.no_plugins, None, 0)
	conf_define_from_opt('HAVE_SOCKET', not Options.options.no_socket, None, 0)
	conf_define_from_opt('HAVE_VTE', not Options.options.no_vte, None, 0)

	conf.write_config_header('config.h')

	Utils.pprint('BLUE', 'Summary:')
	print_message(conf, 'Install Geany ' + VERSION + ' in', conf.env['PREFIX'])
	print_message(conf, 'Using GTK version', gtk_version)
	print_message(conf, 'Build with GTK printing support', have_gtk_210 and 'yes' or 'no')
	print_message(conf, 'Build with plugin support', Options.options.no_plugins and 'no' or 'yes')
	print_message(conf, 'Use virtual terminal support', Options.options.no_vte and 'no' or 'yes')
	if svn_rev != '-1':
		print_message(conf, 'Compiling Subversion revision', svn_rev)
		conf.env.append_value('CCFLAGS', '-g -DGEANY_DEBUG'.split())

	conf.env.append_value('CCFLAGS', '-DHAVE_CONFIG_H')
	conf.env.append_value('CCFLAGS', '-DGTK') # Scintilla needs this
	# for now define GEANY_PRIVATE for all files, even though it should just be for src/*.
	conf.env.append_value('CCFLAGS', '-DGEANY_PRIVATE')
	# Scintilla flags
	conf.env.append_value('CXXFLAGS',
		'-DNDEBUG -DGTK -DGTK2 -DSCI_LEXER -DG_THREADS_IMPL_NONE'.split())


def set_options(opt):
	opt.tool_options('compiler_cc')
	opt.tool_options('compiler_cxx')
	opt.tool_options('intltool')

	if 'configure' in sys.argv:
		# Features
		opt.add_option('--disable-plugins', action='store_true', default=False,
			help='compile without plugin support [default: No]', dest='no_plugins')
		opt.add_option('--disable-socket', action='store_true', default=False,
			help='compile without support to detect a running instance [[default: No]',
			dest='no_socket')
		opt.add_option('--disable-vte', action='store_true', default=target_is_win32(os.environ),
			help='compile without support for an embedded virtual terminal [[default: No]',
			dest='no_vte')
		opt.add_option('--enable-gnu-regex', action='store_true', default=False,
			help='compile with included GNU regex library [default: No]', dest='gnu_regex')
		# Paths
		opt.add_option('--mandir', type='string', default='',
			help='man documentation', dest='mandir')
		opt.add_option('--docdir', type='string', default='',
			help='documentation root', dest='docdir')
		opt.add_option('--libdir', type='string', default='',
			help='object code libraries', dest='libdir')
	# Actions
	opt.add_option('--htmldoc', action='store_true', default=False,
		help='generate HTML documentation', dest='htmldoc')
	opt.add_option('--hackingdoc', action='store_true', default=False,
		help='generate HTML documentation from HACKING file', dest='hackingdoc')
	opt.add_option('--apidoc', action='store_true', default=False,
		help='generate API reference documentation', dest='apidoc')
	opt.add_option('--update-po', action='store_true', default=False,
		help='update the message catalogs for translation', dest='update_po')


def build(bld):
	is_win32 = target_is_win32(bld.env)

	def build_plugin(plugin_name, install = True):
		if install:
			instpath = '${PREFIX}/lib' if is_win32 else '${LIBDIR}/geany/'
		else:
			instpath = None

		bld.new_task_gen(
			features				= 'cc cshlib',
			source					= 'plugins/' + plugin_name + '.c',
			includes				= '. plugins/ src/ scintilla/include tagmanager/include',
			defines					= 'G_LOG_DOMAIN="%s"' % plugin_name,
			target					= plugin_name,
			uselib					= 'GTK',
			install_path			= instpath
		)


	# Tagmanager
	if bld.env['USE_INCLUDED_REGEX'] == 1:
		tagmanager_sources.append('tagmanager/regex.c')
	bld.new_task_gen(
		features		= 'cc cstaticlib',
		source			= tagmanager_sources,
		name			= 'tagmanager',
		target			= 'tagmanager',
		includes		= '. tagmanager/ tagmanager/include/',
		defines			= 'G_LOG_DOMAIN="Tagmanager"',
		uselib			= 'GTK',
		install_path	= None # do not install this library
	)

	# Scintilla
	bld.new_task_gen(
		features		= 'cc cxx cstaticlib',
		name			= 'scintilla',
		target			= 'scintilla',
		source			= scintilla_sources,
		includes		= 'scintilla/ scintilla/include/',
		uselib			= 'GTK',
		install_path	= None, # do not install this library
	)

	# Geany
	if bld.env['HAVE_VTE'] == 1:
		geany_sources.append('src/vte.c')
	if is_win32:
		geany_sources.append('src/win32.c')

	bld.new_task_gen(
		features		= 'cc cxx cprogram',
		name			= 'geany',
		target			= 'geany',
		source			= geany_sources,
		includes		= '. src/ scintilla/include/ tagmanager/include/',
		defines			= 'G_LOG_DOMAIN="Geany"',
		uselib			= 'GTK GIO X11 WIN32 SUNOS_SOCKET',
		uselib_local	= 'scintilla tagmanager',
		add_objects		= 'geany-rc' if is_win32 else None
	)

	# geanyfunctions.h
	bld.new_task_gen(
		source	= 'plugins/genapi.py src/plugins.c',
		name	= 'geanyfunctions.h',
		before	= 'cc cxx',
		cwd		= '%s/plugins' % bld.path.abspath(),
		rule	= 'python genapi.py -q'
	)

	# Plugins
	if bld.env['HAVE_PLUGINS'] == 1:
		build_plugin('classbuilder')
		build_plugin('demoplugin', False)
		build_plugin('export')
		build_plugin('filebrowser')
		build_plugin('htmlchars')
		build_plugin('saveactions')
		build_plugin('splitwindow', not is_win32)

	# Translations
	if bld.env['INTLTOOL']:
		bld.new_task_gen(
			features		= 'intltool_po',
			podir			= 'po',
			install_path	= '${LOCALEDIR}',
			appname			= 'geany'
		)

	# geany.pc
	bld.new_task_gen(
		features		= 'subst',
		source			= 'geany.pc.in',
		target			= 'geany.pc',
		dict			= { 'VERSION' : VERSION,
							'prefix': bld.env['PREFIX'],
							'exec_prefix': '${prefix}',
							'libdir': '${exec_prefix}/lib',
							'includedir': '${prefix}/include',
							'datarootdir': '${prefix}/share',
							'datadir': '${datarootdir}',
							'localedir': '${datarootdir}/locale' },
		install_path	= None if is_win32 else '${LIBDIR}/pkgconfig'
	)

	if not is_win32:
		# geany.desktop
		if bld.env['INTLTOOL']:
			bld.new_task_gen(
				features		= 'intltool_in',
				source			= 'geany.desktop.in',
				flags			= [ '-d', '-q', '-u', '-c' ],
				install_path	= '${DATADIR}/applications'
			)

		# geany.1
		bld.new_task_gen(
			features		= 'subst',
			source			= 'doc/geany.1.in',
			target			= 'geany.1',
			dict			= { 'VERSION' : VERSION,
								'GEANY_DATA_DIR': bld.env['DATADIR'] + '/geany' },
			install_path	= '${MANDIR}/man1'
		)

		# geany.spec
		bld.new_task_gen(
			features		= 'subst',
			source			= 'geany.spec.in',
			target			= 'geany.spec',
			install_path	= None,
			dict			= { 'VERSION' : VERSION }
		)

		# Doxyfile
		bld.new_task_gen(
			features		= 'subst',
			source			= 'doc/Doxyfile.in',
			target			= 'doc/Doxyfile',
			install_path	= None,
			dict			= { 'VERSION' : VERSION }
		)
	else:
		bld.new_task_gen(
			features		= 'cc',
			name			= 'geany-rc',
			source			= 'geany_private.rc'
		)

	###
	# Install files
	###
	if not is_win32:
		# Headers
		bld.install_files('${PREFIX}/include/geany', '''
			src/document.h src/editor.h src/encodings.h src/filetypes.h src/geany.h
			src/highlighting.h src/keybindings.h src/msgwindow.h src/plugindata.h
			src/prefs.h src/project.h src/search.h src/stash.h src/support.h
			src/templates.h src/toolbar.h src/ui_utils.h src/utils.h
			plugins/geanyplugin.h plugins/geanyfunctions.h''')
		bld.install_files('${PREFIX}/include/geany/scintilla', '''
			scintilla/include/SciLexer.h scintilla/include/Scintilla.h
			scintilla/include/Scintilla.iface scintilla/include/ScintillaWidget.h ''')
		bld.install_files('${PREFIX}/include/geany/tagmanager', '''
			tagmanager/include/tm_file_entry.h tagmanager/include/tm_project.h
			tagmanager/include/tm_source_file.h
			tagmanager/include/tm_symbol.h tagmanager/include/tm_tag.h
			tagmanager/include/tm_tagmanager.h tagmanager/include/tm_work_object.h
			tagmanager/include/tm_workspace.h ''')
	# Docs
	base_dir = '${PREFIX}' if is_win32 else '${DOCDIR}'
	ext = '.txt' if is_win32 else ''
	html_dir = '' if is_win32 else 'html/'
	html_name = 'Manual.html' if is_win32 else 'index.html'
	for f in 'AUTHORS ChangeLog COPYING README NEWS THANKS TODO'.split():
		bld.install_as("%s/%s%s" % (base_dir, uc_first(f, is_win32), ext), f)
	bld.install_files('${DOCDIR}/%simages' % html_dir, 'doc/images/*.png')
	bld.install_as('${DOCDIR}/%s' % uc_first('manual.txt', is_win32), 'doc/geany.txt')
	bld.install_as('${DOCDIR}/%s%s' % (html_dir, html_name), 'doc/geany.html')
	bld.install_as('${DOCDIR}/ScintillaLicense.txt', 'scintilla/License.txt')
	if is_win32:
		bld.install_as('${DOCDIR}/ReadMe.I18n.txt', 'README.I18N')
		bld.install_as('${DOCDIR}/Hacking.txt', 'HACKING')
	# Data
	dir = '' if is_win32 else 'geany'
	bld.install_files('${DATADIR}/%s' % dir, 'data/filetype*')
	bld.install_files('${DATADIR}/%s' % dir, 'data/*.tags')
	bld.install_files('${DATADIR}/%s' % dir, 'data/snippets.conf')
	bld.install_files('${DATADIR}/%s' % dir, 'data/ui_toolbar.xml')
	bld.install_files('${DATADIR}/%s/templates' % dir, 'data/templates/*.*')
	bld.install_files('${DATADIR}/%s/templates/files' % dir, 'data/templates/files/*.*')
	bld.install_as('${DATADIR}/%s/GPL-2' % dir, 'COPYING')
	# Icons
	bld.install_files('${PREFIX}/share/icons'
		if is_win32	else '${DATADIR}/icons/hicolor/16x16/apps', 'icons/16x16/*.png')
	if not is_win32:
		bld.install_files('${DATADIR}/icons/hicolor/48x48/apps', 'icons/48x48/*.png')
		bld.install_files('${DATADIR}/icons/hicolor/scalable/apps', 'icons/scalable/*.svg')


@taskgen
@feature('intltool_po')
def write_linguas_file(self):
	linguas = ''
	if 'LINGUAS' in Build.bld.env:
		files = Build.bld.env['LINGUAS']
		for po_filename in files.split(' '):
			if os.path.exists('po/%s.po' % po_filename):
				linguas += '%s ' % po_filename
	else:
		files = os.listdir('%s/po' % self.path.abspath())
		files.sort()
		for f in files:
			if f.endswith('.po'):
				linguas += '%s ' % f[:-3]
	f = open("po/LINGUAS", "w")
	f.write('# This file is autogenerated. Do not edit.\n%s\n' % linguas)
	f.close()


def shutdown():
	is_win32 = False if not Build.bld else target_is_win32(Build.bld.env)
	# the following code was taken from midori's WAF script, thanks
	if not is_win32 and not Options.options.destdir and (Options.commands['install'] or \
			Options.commands['uninstall']):
		dir = Build.bld.get_install_path('${DATADIR}/icons/hicolor')
		icon_cache_updated = False
		try:
			Utils.exec_command('gtk-update-icon-cache -q -f -t %s' % dir)
			Utils.pprint('GREEN', 'GTK icon cache updated.')
			icon_cache_updated = True
		except:
			Utils.pprint('YELLOW', 'Failed to update icon cache for %s.' % dir)
		if not icon_cache_updated:
			print 'Icon cache not updated. After install, run this:'
			print 'gtk-update-icon-cache -q -f -t %s' % dir

	if Options.options.apidoc:
		doxyfile = os.path.join(Build.bld.srcnode.abspath(Build.bld.env), 'doc', 'Doxyfile')
		cmd = Configure.find_program_impl(Build.bld.env, 'doxygen')
		if cmd:
			os.chdir('doc')
			ret = launch('%s %s' % (cmd, doxyfile), 'Generating API reference documentation')
		else:
			Utils.pprint('RED',
				'doxygen could not be found. Please install the doxygen package.')
			sys.exit(1)

	if Options.options.htmldoc or Options.options.hackingdoc:
		# first try rst2html.py as it is the upstream default, fall back to rst2html
		cmd = Configure.find_program_impl(Build.bld.env, 'rst2html.py')
		if not cmd:
			cmd = Configure.find_program_impl(Build.bld.env, 'rst2html')
		if cmd:
			if Options.options.hackingdoc:
				file_in = '../HACKING'
				file_out = 'hacking.html'
				msg = 'HACKING HTML'
			else:
				file_in = 'geany.txt'
				file_out = 'geany.html'
				msg = 'HTML'
			os.chdir('doc')
			ret = launch(cmd + ' -stg --stylesheet=geany.css %s %s' % (file_in, file_out),
				'Generating %s documentation' % msg)
		else:
			Utils.pprint('RED',
				'rst2html.py could not be found. Please install the Python docutils package.')
			sys.exit(1)

	if Options.options.update_po:
		# the following code was taken from midori's WAF script, thanks
		os.chdir('%s/po' % srcdir)
		try:
			try:
				size_old = os.stat('geany.pot').st_size
			except:
				size_old = 0
			Utils.exec_command('intltool-update --pot -g %s' % APPNAME)
			size_new = os.stat('geany.pot').st_size
			if size_new != size_old:
				Utils.pprint('CYAN', 'Updated POT file.')
				launch('intltool-update -r -g %s' % APPNAME, 'Updating translations', 'CYAN')
			else:
				Utils.pprint('CYAN', 'POT file is up to date.')
		except:
			Utils.pprint('RED', 'Failed to generate pot file.')
		os.chdir('..')


# Simple function to execute a command and print its exit status
def launch(command, status, success_color='GREEN'):
	ret = 0
	Utils.pprint(success_color, status)
	try:
		ret = Utils.exec_command(command)
	except OSError, e:
		ret = 1
		print str(e), ":", command
	except:
		ret = 1

	if ret != 0:
		Utils.pprint('RED', status + ' failed')

	return ret


def print_message(conf, msg, result, color = 'GREEN'):
	conf.check_message_1(msg)
	conf.check_message_2(result, color)


def uc_first(s, is_win32):
	if is_win32:
		return s.title()
	return s


def target_is_win32(env):
	if sys.platform == 'win32':
		return True
	if env and 'CC' in env:
		cc = env['CC']
		if not isinstance(cc, str):
			cc = ''.join(cc)
		return cc.find('mingw') != -1
	return False
