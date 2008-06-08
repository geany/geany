#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# WAF build script - this file is part of Geany, a fast and lightweight IDE
#
# Copyright 2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

Requires WAF SVN r3624 (or later) and Python 2.4 (or later).
"""


import Params, Configure, Runner, Task, Utils, Install
import sys, os, subprocess, shutil


APPNAME = 'geany'
VERSION = '0.15'

srcdir = '.'
blddir = 'build'


def configure(conf):
    def conf_get_svn_rev():
        try:
            p = subprocess.Popen(['svn', 'info', '--non-interactive'], stdout=subprocess.PIPE, \
                    stderr=subprocess.STDOUT, close_fds=False, env={'LANG' : 'C'})
            stdout = p.communicate()[0]

            if p.returncode == 0:
                lines = stdout.splitlines(True)
                for line in lines:
                    if line.startswith('Last Changed Rev'):
                        key, value = line.split(': ', 1)
                        return value.strip()
            return '-1'
        except:
            return '-1'

    def conf_get_pkg_ver(pkgname):
        ret = os.popen('PKG_CONFIG_PATH=$PKG_CONFIG_PATH pkg-config --modversion %s' % pkgname).read().strip()
        if ret:
            return ret
        else:
            return '(unknown)'

    def conf_check_header(header_name, mand = 0):
        headerconf              = conf.create_header_configurator()
        headerconf.name         = header_name
        headerconf.mandatory    = mand
        headerconf.message      = header_name + ' is necessary.'
        headerconf.run()

    # TODO this only checks in header files, not in libraries (fix this in Waf)
    def conf_check_function(func_name, header_files = '', mand = 1):
        functest            = conf.create_function_enumerator()
        functest.headers    = header_files
        functest.mandatory  = mand
        functest.function   = func_name
        functest.define     = 'HAVE_' + func_name.upper()
        functest.run()

    def conf_define_from_opt(define_name, opt_name, default_value, quote=1):
        if opt_name:
            if isinstance(opt_name, bool):
                opt_name = 1
            conf.define(define_name, opt_name, quote)
        elif default_value:
            conf.define(define_name, default_value, quote)

    conf.check_tool('compiler_cc')

    conf_check_header('fcntl.h', 0)
    conf_check_header('fnmatch.h', 0)
    conf_check_header('glob.h', 0)
    conf_check_header('sys/time.h', 0)
    conf_check_header('sys/types.h', 0)
    conf_check_header('sys/stat.h', 0)
    conf.define('HAVE_STDLIB_H', 1) # are there systems without stdlib.h?
    conf.define('STDC_HEADERS', 1) # an optimistic guess ;-)

    if Params.g_options.gnu_regex:
        conf.define('HAVE_REGCOMP', 1, 0)
        conf.define('USE_INCLUDED_REGEX', 1, 0)
    else:
        conf_check_header('regex.h', 0)
        if conf.env['HAVE_REGEX_H'] == 1:
            conf_check_function('regcomp', ['regex.h'], 0)
        # fallback to included regex lib
        if conf.env['HAVE_REGCOMP'] != 1 or conf.env['HAVE_REGEX_H'] != 1:
            conf.define('HAVE_REGCOMP', 1, 0)
            conf.define('USE_INCLUDED_REGEX', 1, 0)
            Params.pprint('YELLOW', 'Using included GNU regex library.')

    conf_check_function('fgetpos', ['stdio.h'], 0)
    conf_check_function('ftruncate', ['unistd.h'], 0)
    conf_check_function('gethostname', ['unistd.h'], 0)
    conf_check_function('mkstemp', ['stdlib.h'], 0)
    conf_check_function('strerror', ['string.h'], 0)
    conf_check_function('strstr', ['string.h'], 1)

    # check for cxx after the header and function checks have been done to ensure they are
    # checked with cc not cxx
    conf.check_tool('compiler_cxx intltool misc')

    # first check for GTK 2.10 for GTK printing message
    conf.check_pkg('gtk+-2.0', destvar='GTK', vnum='2.10.0')
    if conf.env['HAVE_GTK'] == 1:
        have_gtk_210 = True
    else:
        # we don't have GTK >= 2.10, so check for at least 2.6 and fail if not found
        conf.check_pkg('gtk+-2.0', destvar='GTK', vnum='2.6.0', mandatory=True)
        have_gtk_210 = False

    conf_define_from_opt('LIBDIR', Params.g_options.libdir, conf.env['PREFIX'] + '/lib')
    conf_define_from_opt('DOCDIR', Params.g_options.docdir, conf.env['DATADIR'] + '/doc/geany')
    conf_define_from_opt('MANDIR', Params.g_options.mandir, conf.env['DATADIR'] + '/man')

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

    conf_define_from_opt('HAVE_PLUGINS', not Params.g_options.no_plugins, None, 0)
    conf_define_from_opt('HAVE_SOCKET', not Params.g_options.no_socket, None, 0)
    conf_define_from_opt('HAVE_VTE', not Params.g_options.no_vte, None, 0)

    conf.write_config_header('config.h')

    Params.pprint('BLUE', 'Summary:')
    print_message('Install Geany ' + VERSION + ' in', conf.env['PREFIX'])
    print_message('Using GTK version', conf_get_pkg_ver('gtk+-2.0'))
    print_message('Build with GTK printing support', have_gtk_210 and 'yes' or 'no')
    print_message('Build with plugin support', Params.g_options.no_plugins and 'no' or 'yes')
    print_message('Use virtual terminal support', Params.g_options.no_vte and 'no' or 'yes')
    if svn_rev != '-1':
        print_message('Compiling Subversion revision', svn_rev)
        conf.env.append_value('CCFLAGS', '-g -DGEANY_DEBUG')

	conf.env.append_value('CCFLAGS', '-DHAVE_CONFIG_H')
    conf.env.append_value('CXXFLAGS', ' -DNDEBUG -Os -DGTK -DGTK2 -DSCI_LEXER -DG_THREADS_IMPL_NONE')


def set_options(opt):
    opt.tool_options('compiler_cc')
    opt.tool_options('compiler_cxx')
    opt.tool_options('intltool')

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
    def build_plugin(plugin_name, local_inst_var = 'LIBDIR'):
        obj                         = bld.new_task_gen('cc', 'shlib')
        obj.source                  = 'plugins/' + plugin_name + '.c'
        obj.includes                = '. plugins/ src/ scintilla/include tagmanager/include'
        obj.env['shlib_PATTERN']    = '%s.so'
        obj.target                  = plugin_name
        obj.uselib                  = 'GTK'
        obj.inst_var                = local_inst_var
        obj.inst_dir                = '/geany/'
        #~ obj.want_libtool         = 1

    # Tagmanager
    if bld.env['USE_INCLUDED_REGEX'] == 1:
        excludes = ''
    else:
        excludes = ['regex.c']
    obj = bld.new_task_gen('cc', 'staticlib')
    obj.find_sources_in_dirs('tagmanager/', excludes)
    obj.name        = 'tagmanager'
    obj.target      = 'tagmanager'
    obj.includes    = '. tagmanager/ tagmanager/include/'
    obj.uselib      = 'GTK'
    obj.inst_var    = 0 # do not install this library

    # Scintilla
    obj = bld.new_task_gen('cxx', 'staticlib')
    obj.features.append('cc')
    obj.find_sources_in_dirs('scintilla/')
    obj.name            = 'scintilla'
    obj.target          = 'scintilla'
    obj.includes        = 'scintilla/ scintilla/include/'
    obj.uselib          = 'GTK'
    obj.inst_var        = 0 # do not install this library

    # Geany
    excludes = ['win32.c', 'gb.c', 'images.c']
    if bld.env['HAVE_VTE'] != 1:
        excludes.append('vte.c')
    obj = bld.new_task_gen('cxx', 'program')
    obj.features.append('cc')
    obj.find_sources_in_dirs('src/', excludes)
    obj.name            = 'geany'
    obj.target          = 'geany'
    obj.includes        = '. src/ scintilla/include/ tagmanager/include/'
    obj.uselib          = 'GTK'
    obj.uselib_local    = 'scintilla tagmanager'

    # Plugins
    if bld.env['HAVE_PLUGINS'] == 1:
        build_plugin('autosave')
        build_plugin('classbuilder')
        build_plugin('demoplugin', 0)
        build_plugin('export')
        build_plugin('filebrowser')
        build_plugin('htmlchars')
        build_plugin('vcdiff')

    # Translations
    obj         = bld.new_task_gen('intltool_po')
    obj.podir   = 'po'
    obj.appname = 'geany'

    # geany.desktop
    obj         = bld.new_task_gen('intltool_in')
    obj.source  = 'geany.desktop.in'
    obj.inst_var = 'DATADIR'
    obj.inst_dir  = 'applications'
    obj.flags   = '-d'

    # geany.pc
    obj         = bld.new_task_gen('subst')
    obj.source  = 'geany.pc.in'
    obj.target  = 'geany.pc'
    obj.dict    = { 'VERSION' : VERSION,
                    'prefix': bld.env['PREFIX'],
                    'exec_prefix': '${prefix}',
                    'libdir': '${exec_prefix}/lib',
                    'includedir': '${prefix}/include',
                    'datarootdir': '${prefix}/share',
                    'datadir': '${datarootdir}',
                    'localedir': '${datarootdir}/locale' }

    # geany.1
    obj         = bld.new_task_gen('subst')
    obj.source  = 'doc/geany.1.in'
    obj.target  = 'geany.1'
    obj.dict    = { 'VERSION' : VERSION,
                    'GEANY_DATA_DIR': bld.env['DATADIR'] + '/geany' }

    # geany.spec
    obj          = bld.new_task_gen('subst')
    obj.source   = 'geany.spec.in'
    obj.target   = 'geany.spec'
    obj.inst_var = 0
    obj.dict     = { 'VERSION' : VERSION }

    # Doxyfile
    obj          = bld.new_task_gen('subst')
    obj.source   = 'doc/Doxyfile.in'
    obj.target   = 'Doxyfile'
    obj.inst_var = 0
    obj.dict     = { 'VERSION' : VERSION }

    ###
    # Install files
    ###
    bld.install_files('LIBDIR', 'pkgconfig', 'geany.pc')
    # Headers
    bld.install_files('PREFIX', 'include/geany', '''
        src/dialogs.h src/document.h src/editor.h src/encodings.h src/filetypes.h src/geany.h
        src/highlighting.h src/keybindings.h src/msgwindow.h src/plugindata.h src/plugins.h
        src/prefs.h src/project.h src/sciwrappers.h src/search.h src/support.h src/templates.h
        src/ui_utils.h src/utils.h
        plugins/pluginmacros.h ''')
    bld.install_files('PREFIX', 'include/geany/scintilla', '''
        scintilla/include/SciLexer.h scintilla/include/Scintilla.h scintilla/include/Scintilla.iface
        scintilla/include/ScintillaWidget.h ''')
    bld.install_files('PREFIX', 'include/geany/tagmanager', '''
        tagmanager/include/tm_file_entry.h tagmanager/include/tm_project.h tagmanager/include/tm_source_file.h
        tagmanager/include/tm_symbol.h tagmanager/include/tm_tag.h tagmanager/include/tm_tagmanager.h
        tagmanager/include/tm_work_object.h tagmanager/include/tm_workspace.h ''')
    # Docs
    bld.install_files('MANDIR', 'man1', 'doc/geany.1')
    bld.install_files('DOCDIR', '', 'AUTHORS ChangeLog COPYING README NEWS THANKS TODO')
    bld.install_files('DOCDIR', 'html/images', 'doc/images/*.png')
    bld.install_as('DOCDIR', 'manual.txt', 'doc/geany.txt')
    bld.install_as('DOCDIR', 'html/index.html', 'doc/geany.html')
    bld.install_as('DOCDIR', 'ScintillaLicense.txt', 'scintilla/License.txt')
    # Data
    bld.install_files('DATADIR', 'geany', 'data/filetype*')
    bld.install_files('DATADIR', 'geany', 'data/*.tags')
    bld.install_files('DATADIR', 'geany', 'data/snippets.conf')
    bld.install_as('DATADIR', 'geany/GPL-2', 'COPYING')
    # Icons
    bld.install_files('DATADIR', 'pixmaps', 'pixmaps/geany.png')
    bld.install_files('DATADIR', 'icons/hicolor/16x16/apps', 'icons/16x16/*.png')


def shutdown():
    # the following code was taken from midori's WAF script, thanks
    if Params.g_commands['install'] or Params.g_commands['uninstall']:
        dir = Install.path_install('DATADIR', 'icons/hicolor')
        icon_cache_updated = False
        if not Params.g_options.destdir:
            try:
                subprocess.call(['gtk-update-icon-cache', '-q', '-f', '-t', dir])
                Params.pprint('GREEN', 'Updated GTK icon cache.')
                icon_cache_updated = True
            except:
                Params.pprint('YELLOW', 'Failed to update icon cache for ' + dir + '.')
        if not icon_cache_updated and not Params.g_options.destdir:
            print 'Icon cache not updated. After install, run this:'
            print 'gtk-update-icon-cache -q -f -t %s' % dir

    if Params.g_options.apidoc:
        doxyfile = os.path.join(Params.g_build.m_srcnode.abspath( \
            Params.g_build.env()), 'doc', 'Doxyfile')
        os.chdir('doc')
        launch('doxygen ' + doxyfile, 'Generating API reference documentation')

    if Params.g_options.htmldoc:
        os.chdir('doc')
        launch('rst2html -stg --stylesheet=geany.css geany.txt geany.html',
            'Generating HTML documentation')

    if Params.g_options.update_po:
        # the following code was taken from midori's WAF script, thanks
        os.chdir('./po')
        try:
            try:
                size_old = os.stat('geany.pot').st_size
            except:
                size_old = 0
            subprocess.call(['intltool-update', '--pot'])
            size_new = os.stat('geany.pot').st_size
            if size_new != size_old:
                Params.pprint('CYAN', 'Updated POT file.')
                launch('intltool-update -r', 'Updating translations', 'CYAN')
            else:
                Params.pprint('CYAN', 'POT file is up to date.')
        except:
            Params.pprint('RED', 'Failed to generate pot file.')
        os.chdir('..')


# Simple function to execute a command and print its exit status
def launch(command, status, success_color='GREEN'):
    ret = 0
    Params.pprint(success_color, status)
    try:
        ret = subprocess.call(command.split())
    except:
        ret = 1

    if ret != 0:
        Params.pprint('RED', status + ' failed')

    return ret


def print_message(msg, result, color = 'GREEN'):
    Configure.g_maxlen = max(Configure.g_maxlen, len(msg))
    print "%s :" % msg.ljust(Configure.g_maxlen),
    Params.pprint(color, result)
    Runner.print_log(msg, '\n\n')

