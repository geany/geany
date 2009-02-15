#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# WAF build script - this file is part of Geany, a fast and lightweight IDE
#
# Copyright 2008-2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
# Copyright 2008-2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

Requires WAF 1.5.3 and Python 2.4 (or later).
"""


import Build, Configure, Options, Utils
import sys, os, shutil
from distutils import version


APPNAME = 'geany'
VERSION = '0.17'

srcdir = '.'
blddir = '_build_'


tagmanager_sources = [
	'tagmanager/args.c', 'tagmanager/asm.c', 'tagmanager/basic.c', 'tagmanager/c.c',
	'tagmanager/conf.c', 'tagmanager/css.c', 'tagmanager/ctags.c', 'tagmanager/diff.c',
	'tagmanager/docbook.c', 'tagmanager/entry.c', 'tagmanager/fortran.c', 'tagmanager/get.c',
	'tagmanager/haskell.c', 'tagmanager/haxe.c', 'tagmanager/html.c', 'tagmanager/js.c',
	'tagmanager/keyword.c', 'tagmanager/latex.c', 'tagmanager/lregex.c', 'tagmanager/lua.c',
	'tagmanager/make.c', 'tagmanager/matlab.c', 'tagmanager/options.c', 'tagmanager/parse.c',
	'tagmanager/pascal.c',
	'tagmanager/perl.c', 'tagmanager/php.c', 'tagmanager/python.c', 'tagmanager/read.c',
	'tagmanager/rest.c', 'tagmanager/ruby.c', 'tagmanager/sh.c', 'tagmanager/sort.c',
	'tagmanager/sql.c', 'tagmanager/strlist.c', 'tagmanager/tcl.c', 'tagmanager/tm_file_entry.c',
	'tagmanager/tm_project.c', 'tagmanager/tm_source_file.c', 'tagmanager/tm_symbol.c',
	'tagmanager/tm_tag.c', 'tagmanager/tm_tagmanager.c', 'tagmanager/tm_work_object.c',
	'tagmanager/tm_workspace.c', 'tagmanager/vhdl.c', 'tagmanager/vstring.c' ]

scintilla_sources = [
	'scintilla/AutoComplete.cxx', 'scintilla/CallTip.cxx', 'scintilla/CellBuffer.cxx',
	'scintilla/CharClassify.cxx', 'scintilla/ContractionState.cxx', 'scintilla/Decoration.cxx',
	'scintilla/DocumentAccessor.cxx', 'scintilla/Document.cxx', 'scintilla/Editor.cxx',
	'scintilla/ExternalLexer.cxx',  'scintilla/Indicator.cxx',  'scintilla/KeyMap.cxx',
	'scintilla/KeyWords.cxx', 'scintilla/LexAda.cxx', 'scintilla/LexAsm.cxx', 'scintilla/LexBash.cxx',
	'scintilla/LexBasic.cxx', 'scintilla/LexCaml.cxx', 'scintilla/LexCmake.cxx', 'scintilla/LexCPP.cxx',
	'scintilla/LexCrontab.cxx', 'scintilla/LexCSS.cxx', 'scintilla/LexD.cxx',
	'scintilla/LexFortran.cxx', 'scintilla/LexHaskell.cxx', 'scintilla/LexHTML.cxx',
	'scintilla/LexLua.cxx', 'scintilla/LexNsis.cxx', 'scintilla/LexOthers.cxx',
	'scintilla/LexPascal.cxx', 'scintilla/LexPerl.cxx', 'scintilla/LexPython.cxx',
	'scintilla/LexR.cxx', 'scintilla/LexMatlab.cxx', 'scintilla/LexYAML.cxx',
	'scintilla/LexRuby.cxx', 'scintilla/LexSQL.cxx', 'scintilla/LexTCL.cxx',
	'scintilla/LexVHDL.cxx', 'scintilla/LineMarker.cxx', 'scintilla/PlatGTK.cxx',
	'scintilla/PositionCache.cxx', 'scintilla/PropSet.cxx', 'scintilla/RESearch.cxx',
	'scintilla/RunStyles.cxx', 'scintilla/ScintillaBase.cxx', 'scintilla/ScintillaGTK.cxx',
	'scintilla/scintilla-marshal.c', 'scintilla/StyleContext.cxx', 'scintilla/Style.cxx',
	'scintilla/UniConversion.cxx', 'scintilla/ViewStyle.cxx', 'scintilla/WindowAccessor.cxx',
	'scintilla/XPM.cxx' ]

geany_sources = [
	'src/about.c', 'src/build.c', 'src/callbacks.c', 'src/dialogs.c', 'src/document.c',
	'src/editor.c', 'src/encodings.c', 'src/filetypes.c', 'src/geanyentryaction.c',
	'src/geanymenubuttonaction.c', 'src/geanyobject.c', 'src/geanywraplabel.c',
	'src/highlighting.c', 'src/interface.c', 'src/keybindings.c',
	'src/keyfile.c', 'src/log.c', 'src/main.c', 'src/msgwindow.c', 'src/navqueue.c', 'src/notebook.c',
	'src/plugins.c', 'src/prefix.c', 'src/prefs.c', 'src/printing.c', 'src/project.c',
	'src/queue.c', 'src/sciwrappers.c', 'src/search.c', 'src/socket.c', 'src/stash.c',
	'src/symbols.c',
	'src/templates.c', 'src/toolbar.c', 'src/tools.c', 'src/treeviews.c',
	'src/ui_utils.c', 'src/utils.c' ]



def configure(conf):
	def conf_get_svn_rev():
		# try GIT
		if os.path.exists('.git'):
			cmds = [ 'git svn find-rev HEAD 2>/dev/null',
					 'git svn find-rev origin/trunk 2>/dev/null',
					 'git svn find-rev trunk 2>/dev/null',
					 'git svn find-rev master 2>/dev/null' ]
			for c in cmds:
				try:
					stdout = Utils.cmd_output(c)
					if stdout:
						return stdout.strip()
				except:
					pass
		# try SVN
		elif os.path.exists('.svn'):
			try:
				stdout = Utils.cmd_output(cmd='svn info --non-interactive', env={'LANG' : 'C'})
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

	# check for cxx after the header and function checks have been done to ensure they are
	# checked with cc not cxx
	conf.check_tool('compiler_cxx intltool misc')

	conf.check_cfg(package='gtk+-2.0', atleast_version='2.8.0', uselib_store='GTK',
		mandatory=True, args='--cflags --libs')

	# GTK version check
	have_gtk_210 = False
	gtk_version = conf.check_cfg(modversion='gtk+-2.0', uselib_store='GTK')
	if gtk_version:
		if version.LooseVersion(gtk_version) >= version.LooseVersion('2.10.0'):
			have_gtk_210 = True
	else:
		gtk_version = 'Unknown'
	# GIO check
	conf.check_cfg(package='gio-2.0', uselib_store='GIO', args='--cflags --libs', mandatory=False)

	conf_define_from_opt('LIBDIR', Options.options.libdir, conf.env['PREFIX'] + '/lib')
	conf_define_from_opt('DOCDIR', Options.options.docdir, conf.env['DATADIR'] + '/doc/geany')
	conf_define_from_opt('MANDIR', Options.options.mandir, conf.env['DATADIR'] + '/man')

	svn_rev = conf_get_svn_rev()
	conf.define('ENABLE_NLS', 1)
	conf.define('GEANY_LOCALEDIR', 'LOCALEDIR', 0)
	conf.define('GEANY_DATADIR', 'DATADIR', 0)
	conf.define('GEANY_DOCDIR', 'DOCDIR', 0)
	conf.define('GEANY_LIBDIR', 'LIBDIR', 0)
	conf.define('GEANY_PREFIX', conf.env['PREFIX'], 1)
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
	# Scintilla flags
	conf.env.append_value('CXXFLAGS', '-DNDEBUG -DGTK -DGTK2 -DSCI_LEXER -DG_THREADS_IMPL_NONE'.split())


def set_options(opt):
	opt.tool_options('compiler_cc')
	opt.tool_options('compiler_cxx')
	opt.tool_options('intltool')

	if 'configure' in sys.argv:
		# Features
		opt.add_option('--disable-plugins', action='store_true', default=False,
			help='compile without plugin support [default: No]', dest='no_plugins')
		opt.add_option('--disable-socket', action='store_true', default=False,
			help='compile without support to detect a running instance [[default: No]', dest='no_socket')
		opt.add_option('--disable-vte', action='store_true', default=False,
			help='compile without support for an embedded virtual terminal [[default: No]', dest='no_vte')
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
	opt.add_option('--apidoc', action='store_true', default=False,
		help='generate API reference documentation', dest='apidoc')
	opt.add_option('--update-po', action='store_true', default=False,
		help='update the message catalogs for translation', dest='update_po')


def build(bld):
	def build_plugin(plugin_name, install = True):
		if install:
			instpath = '${LIBDIR}/geany/'
		else:
			instpath = None

		bld.new_task_gen(
			features				='cc cshlib',
			source					= 'plugins/' + plugin_name + '.c',
			includes				= '. plugins/ src/ scintilla/include tagmanager/include',
			target					= plugin_name,
			uselib					= 'GTK',
			install_path			= instpath,
		)
		bld.env['shlib_PATTERN']	= '%s.so'


	# Tagmanager
	if bld.env['USE_INCLUDED_REGEX'] == 1:
		tagmanager_sources.append('tagmanager/regex.c')
	bld.new_task_gen(
		features		= 'cc cstaticlib',
		source			= tagmanager_sources,
		name			= 'tagmanager',
		target			= 'tagmanager',
		includes		= '. tagmanager/ tagmanager/include/',
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
	if sys.platform == "win32":
		geany_sources.append('src/win32.c')
	bld.new_task_gen(
		features		= 'cc cxx cprogram',
		name			= 'geany',
		target			= 'geany',
		source			= geany_sources,
		includes		= '. src/ scintilla/include/ tagmanager/include/',
		uselib			= 'GTK GIO',
		uselib_local	= 'scintilla tagmanager'
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
		build_plugin('splitwindow')
		build_plugin('vcdiff')

	# Translations
	bld.new_task_gen(
		features		= 'intltool_po',
		podir			= 'po',
		appname			= 'geany'
	)

	bld.new_task_gen(
		features		= 'intltool_in',
		source			= 'geany.desktop.in',
		flags			= '-d',
		install_path	= '${DATADIR}/applications'
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
		install_path	= '${LIBDIR}/pkgconfig'
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

	###
	# Install files
	###
	# Headers
	bld.install_files('${PREFIX}/include/geany', '''
		src/dialogs.h src/document.h src/editor.h src/encodings.h src/filetypes.h src/geany.h
		src/highlighting.h src/keybindings.h src/msgwindow.h src/plugindata.h src/plugins.h
		src/prefs.h src/project.h src/sciwrappers.h src/search.h src/support.h src/templates.h
		src/toolbar.h src/ui_utils.h src/utils.h
		plugins/pluginmacros.h plugins/geanyfunctions.h ''')
	bld.install_files('${PREFIX}/include/geany/scintilla', '''
		scintilla/include/SciLexer.h scintilla/include/Scintilla.h scintilla/include/Scintilla.iface
		scintilla/include/ScintillaWidget.h ''')
	bld.install_files('${PREFIX}/include/geany/tagmanager', '''
		tagmanager/include/tm_file_entry.h tagmanager/include/tm_project.h tagmanager/include/tm_source_file.h
		tagmanager/include/tm_symbol.h tagmanager/include/tm_tag.h tagmanager/include/tm_tagmanager.h
		tagmanager/include/tm_work_object.h tagmanager/include/tm_workspace.h ''')
	# Docs
	bld.install_files('${DOCDIR}', 'AUTHORS ChangeLog COPYING README NEWS THANKS TODO')
	bld.install_files('${DOCDIR}/html/images', 'doc/images/*.png')
	bld.install_as('${DOCDIR}/manual.txt', 'doc/geany.txt')
	bld.install_as('${DOCDIR}/html/index.html', 'doc/geany.html')
	bld.install_as('${DOCDIR}/ScintillaLicense.txt', 'scintilla/License.txt')
	# Data
	bld.install_files('${DATADIR}/geany', 'data/filetype*')
	bld.install_files('${DATADIR}/geany', 'data/*.tags')
	bld.install_files('${DATADIR}/geany', 'data/snippets.conf')
	bld.install_files('${DATADIR}/geany', 'data/ui_toolbar.xml')
	bld.install_as('${DATADIR}/geany/GPL-2', 'COPYING')
	# Icons
	bld.install_files('${DATADIR}/pixmaps', 'pixmaps/geany.png')
	bld.install_files('${DATADIR}/icons/hicolor/16x16/apps', 'icons/16x16/*.png')


def shutdown():
	# the following code was taken from midori's WAF script, thanks
	if Options.commands['install'] or Options.commands['uninstall']:
		dir = Build.bld.get_install_path('${DATADIR}/icons/hicolor')
		icon_cache_updated = False
		if not Options.options.destdir:
			try:
				Utils.exec_command('gtk-update-icon-cache -q -f -t %s' % dir)
				Utils.pprint('GREEN', 'GTK icon cache updated.')
				icon_cache_updated = True
			except:
				Utils.pprint('YELLOW', 'Failed to update icon cache for ' + dir + '.')
		if not icon_cache_updated and not Options.options.destdir:
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

	if Options.options.htmldoc:
		# first try rst2html.py as it is the upstream default, fall back to rst2html
		cmd = Configure.find_program_impl(Build.bld.env, 'rst2html.py')
		if not cmd:
			cmd = Configure.find_program_impl(Build.bld.env, 'rst2html')
		if cmd:
			os.chdir('doc')
			ret = launch(cmd + ' -stg --stylesheet=geany.css geany.txt geany.html',
				'Generating HTML documentation')
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

