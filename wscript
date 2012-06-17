# -*- coding: utf-8 -*-
#
# WAF build script - this file is part of Geany, a fast and lightweight IDE
#
# Copyright 2008-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
# Copyright 2008-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

Requires WAF 1.6.1 and Python 2.5 (or later).
"""


import sys
import os
import tempfile
from waflib import Logs, Options, Scripting, Utils
from waflib.Configure import ConfigurationContext
from waflib.Errors import WafError
from waflib.TaskGen import feature


APPNAME = 'geany'
VERSION = '1.22'
LINGUAS_FILE = 'po/LINGUAS'
MINIMUM_GTK_VERSION = '2.16.0'
MINIMUM_GLIB_VERSION = '2.20.0'

top = '.'
out = '_build_'


mio_sources = set(['tagmanager/mio/mio.c'])

tagmanager_sources = set([
    'tagmanager/args.c', 'tagmanager/abc.c', 'tagmanager/actionscript.c', 'tagmanager/asm.c',
    'tagmanager/basic.c', 'tagmanager/c.c', 'tagmanager/cobol.c',
    'tagmanager/conf.c', 'tagmanager/css.c', 'tagmanager/ctags.c', 'tagmanager/diff.c',
    'tagmanager/docbook.c', 'tagmanager/entry.c', 'tagmanager/fortran.c', 'tagmanager/get.c',
    'tagmanager/haskell.c', 'tagmanager/haxe.c', 'tagmanager/html.c', 'tagmanager/js.c',
    'tagmanager/keyword.c', 'tagmanager/latex.c', 'tagmanager/lregex.c', 'tagmanager/lua.c',
    'tagmanager/make.c', 'tagmanager/markdown.c', 'tagmanager/matlab.c', 'tagmanager/nsis.c',
    'tagmanager/nestlevel.c', 'tagmanager/objc.c', 'tagmanager/options.c',
    'tagmanager/parse.c', 'tagmanager/pascal.c', 'tagmanager/r.c',
    'tagmanager/perl.c', 'tagmanager/php.c', 'tagmanager/python.c', 'tagmanager/read.c',
    'tagmanager/rest.c', 'tagmanager/ruby.c', 'tagmanager/sh.c', 'tagmanager/sort.c',
    'tagmanager/sql.c', 'tagmanager/strlist.c', 'tagmanager/txt2tags.c', 'tagmanager/tcl.c',
    'tagmanager/tm_file_entry.c',
    'tagmanager/tm_project.c', 'tagmanager/tm_source_file.c', 'tagmanager/tm_symbol.c',
    'tagmanager/tm_tag.c', 'tagmanager/tm_tagmanager.c', 'tagmanager/tm_work_object.c',
    'tagmanager/tm_workspace.c', 'tagmanager/vhdl.c', 'tagmanager/verilog.c', 'tagmanager/vstring.c'])

scintilla_sources = set(['scintilla/gtk/scintilla-marshal.c'])

geany_sources = set([
    'src/about.c', 'src/build.c', 'src/callbacks.c', 'src/dialogs.c', 'src/document.c',
    'src/editor.c', 'src/encodings.c', 'src/filetypes.c', 'src/geanyentryaction.c',
    'src/geanymenubuttonaction.c', 'src/geanyobject.c', 'src/geanywraplabel.c',
    'src/highlighting.c', 'src/keybindings.c',
    'src/keyfile.c', 'src/log.c', 'src/main.c', 'src/msgwindow.c', 'src/navqueue.c', 'src/notebook.c',
    'src/plugins.c', 'src/pluginutils.c', 'src/prefix.c', 'src/prefs.c', 'src/printing.c', 'src/project.c',
    'src/sciwrappers.c', 'src/search.c', 'src/socket.c', 'src/stash.c',
    'src/symbols.c',
    'src/templates.c', 'src/toolbar.c', 'src/tools.c', 'src/sidebar.c',
    'src/ui_utils.c', 'src/utils.c'])


def configure(conf):

    conf.check_waf_version(mini='1.6.1')

    conf.load('compiler_c')
    is_win32 = _target_is_win32(conf)

    conf.check_cc(header_name='fcntl.h', mandatory=False)
    conf.check_cc(header_name='fnmatch.h', mandatory=False)
    conf.check_cc(header_name='glob.h', mandatory=False)
    conf.check_cc(header_name='sys/time.h', mandatory=False)
    conf.check_cc(header_name='sys/types.h', mandatory=False)
    conf.check_cc(header_name='sys/stat.h', mandatory=False)
    conf.define('HAVE_STDLIB_H', 1)  # are there systems without stdlib.h?
    conf.define('STDC_HEADERS', 1)  # an optimistic guess ;-)
    _add_to_env_and_define(conf, 'HAVE_REGCOMP', 1)  # needed for CTags

    conf.check_cc(function_name='fgetpos', header_name='stdio.h', mandatory=False)
    conf.check_cc(function_name='ftruncate', header_name='unistd.h', mandatory=False)
    conf.check_cc(function_name='gethostname', header_name='unistd.h', mandatory=False)
    conf.check_cc(function_name='mkstemp', header_name='stdlib.h', mandatory=False)
    conf.check_cc(function_name='strstr', header_name='string.h')

    # check sunOS socket support
    if Options.platform == 'sunos':
        conf.check_cc(function_name='socket', lib='socket',
                      header_name='sys/socket.h', uselib_store='SUNOS_SOCKET', mandatory=True)

    # check for cxx after the header and function checks have been done to ensure they are
    # checked with cc not cxx
    conf.load('compiler_cxx')
    if is_win32:
        conf.load('winres')
    _load_intltool_if_available(conf)

    # GTK / GIO version check
    conf.check_cfg(package='gtk+-2.0', atleast_version=MINIMUM_GTK_VERSION, uselib_store='GTK',
        mandatory=True, args='--cflags --libs')
    conf.check_cfg(package='glib-2.0', atleast_version=MINIMUM_GLIB_VERSION, uselib_store='GLIB',
        mandatory=True, args='--cflags --libs')
    conf.check_cfg(package='gmodule-2.0', uselib_store='GMODULE',
        mandatory=True, args='--cflags --libs')
    conf.check_cfg(package='gio-2.0', uselib_store='GIO', args='--cflags --libs', mandatory=True)
    gtk_version = conf.check_cfg(modversion='gtk+-2.0', uselib_store='GTK') or 'Unknown'
    conf.check_cfg(package='gthread-2.0', uselib_store='GTHREAD', args='--cflags --libs')

    # Windows specials
    if is_win32:
        if conf.env['PREFIX'].lower() == tempfile.gettempdir().lower():
            # overwrite default prefix on Windows (tempfile.gettempdir() is the Waf default)
            new_prefix = os.path.join(str(conf.root), '%s-%s' % (APPNAME, VERSION))
            _add_to_env_and_define(conf, 'PREFIX', new_prefix, quote=True)
            _add_to_env_and_define(conf, 'BINDIR', os.path.join(new_prefix, 'bin'), quote=True)
        _add_to_env_and_define(conf, 'DOCDIR', os.path.join(conf.env['PREFIX'], 'doc'), quote=True)
        _add_to_env_and_define(conf, 'LIBDIR', conf.env['PREFIX'], quote=True)
        conf.define('LOCALEDIR', os.path.join('share' 'locale'), quote=True)
        # overwrite LOCALEDIR to install message catalogues properly
        conf.env['LOCALEDIR'] = os.path.join(conf.env['PREFIX'], 'share/locale')
        # DATADIR is defined in objidl.h, so we remove it from config.h but keep it in env
        conf.undefine('DATADIR')
        conf.env['DATADIR'] = os.path.join(conf.env['PREFIX'], 'data')
        conf.env.append_value('LINKFLAGS_cprogram', ['-mwindows'])
        conf.env.append_value('LIB_WIN32', ['wsock32', 'uuid', 'ole32', 'iberty'])
    else:
        conf.env['cshlib_PATTERN'] = '%s.so'
        # DATADIR and LOCALEDIR are defined by the intltool tool
        # but they are not added to the environment, so we need to
        _add_define_to_env(conf, 'DATADIR')
        _add_define_to_env(conf, 'LOCALEDIR')
        docdir = os.path.join(conf.env['DATADIR'], 'doc', 'geany')
        libdir = os.path.join(conf.env['PREFIX'], 'lib')
        mandir = os.path.join(conf.env['DATADIR'], 'man')
        _define_from_opt(conf, 'DOCDIR', conf.options.docdir, docdir)
        _define_from_opt(conf, 'LIBDIR', conf.options.libdir, libdir)
        _define_from_opt(conf, 'MANDIR', conf.options.mandir, mandir)

    revision = _get_git_rev(conf)

    conf.define('ENABLE_NLS', 1)
    conf.define('GEANY_LOCALEDIR', '' if is_win32 else conf.env['LOCALEDIR'], quote=True)
    conf.define('GEANY_DATADIR', 'data' if is_win32 else conf.env['DATADIR'], quote=True)
    conf.define('GEANY_DOCDIR', conf.env['DOCDIR'], quote=True)
    conf.define('GEANY_LIBDIR', '' if is_win32 else conf.env['LIBDIR'], quote=True)
    conf.define('GEANY_PREFIX', '' if is_win32 else conf.env['PREFIX'], quote=True)
    conf.define('PACKAGE', APPNAME, quote=True)
    conf.define('VERSION', VERSION, quote=True)
    conf.define('REVISION', revision or '-1', quote=True)

    conf.define('GETTEXT_PACKAGE', APPNAME, quote=True)

    # no VTE on Windows
    if is_win32:
        conf.options.no_vte = True

    _define_from_opt(conf, 'HAVE_PLUGINS', not conf.options.no_plugins, None)
    _define_from_opt(conf, 'HAVE_SOCKET', not conf.options.no_socket, None)
    _define_from_opt(conf, 'HAVE_VTE', not conf.options.no_vte, None)

    conf.write_config_header('config.h', remove=False)

    # some more compiler flags
    conf.env.append_value('CFLAGS', ['-DHAVE_CONFIG_H'])
    if revision is not None:
        conf.env.append_value('CFLAGS', ['-g', '-DGEANY_DEBUG'])
    # Scintilla flags
    conf.env.append_value('CFLAGS', ['-DGTK'])
    conf.env.append_value('CXXFLAGS',
        ['-DNDEBUG', '-DGTK', '-DSCI_LEXER', '-DG_THREADS_IMPL_NONE'])

    # summary
    Logs.pprint('BLUE', 'Summary:')
    conf.msg('Install Geany ' + VERSION + ' in', conf.env['PREFIX'])
    conf.msg('Using GTK version', gtk_version)
    conf.msg('Build with plugin support', conf.options.no_plugins and 'no' or 'yes')
    conf.msg('Use virtual terminal support', conf.options.no_vte and 'no' or 'yes')
    if revision is not None:
        conf.msg('Compiling Git revision', revision)


def options(opt):
    opt.tool_options('compiler_cc')
    opt.tool_options('compiler_cxx')
    opt.tool_options('intltool')

    # Features
    opt.add_option('--disable-plugins', action='store_true', default=False,
        help='compile without plugin support [default: No]', dest='no_plugins')
    opt.add_option('--disable-socket', action='store_true', default=False,
        help='compile without support to detect a running instance [[default: No]',
        dest='no_socket')
    opt.add_option('--disable-vte', action='store_true', default=False,
        help='compile without support for an embedded virtual terminal [[default: No]',
        dest='no_vte')
    # Paths
    opt.add_option('--mandir', type='string', default='',
        help='man documentation', dest='mandir')
    opt.add_option('--docdir', type='string', default='',
        help='documentation root', dest='docdir')
    opt.add_option('--libdir', type='string', default='',
        help='object code libraries', dest='libdir')
    # Actions
    opt.add_option('--hackingdoc', action='store_true', default=False,
        help='generate HTML documentation from HACKING file', dest='hackingdoc')


def build(bld):
    is_win32 = _target_is_win32(bld)

    if bld.cmd == 'clean':
        _remove_linguas_file()
    if bld.cmd in ('install', 'uninstall'):
        bld.add_post_fun(_post_install)

    def build_plugin(plugin_name, install=True):
        if install:
            instpath = '${PREFIX}/lib' if is_win32 else '${LIBDIR}/geany'
        else:
            instpath = None

        bld.new_task_gen(
            features                = ['c', 'cshlib'],
            source                  = 'plugins/%s.c' % plugin_name,
            includes                = ['.', 'src/', 'scintilla/include', 'tagmanager/include'],
            defines                 = 'G_LOG_DOMAIN="%s"' % plugin_name,
            target                  = plugin_name,
            uselib                  = ['GTK', 'GLIB', 'GMODULE'],
            install_path            = instpath)

    # Tagmanager
    bld.new_task_gen(
        features        = ['c', 'cstlib'],
        source          = tagmanager_sources,
        name            = 'tagmanager',
        target          = 'tagmanager',
        includes        = ['.', 'tagmanager', 'tagmanager/include'],
        defines         = 'G_LOG_DOMAIN="Tagmanager"',
        uselib          = ['GTK', 'GLIB'],
        install_path    = None)  # do not install this library

    # MIO
    bld.new_task_gen(
        features        = ['c', 'cstlib'],
        source          = mio_sources,
        name            = 'mio',
        target          = 'mio',
        includes        = ['.', 'tagmanager/mio/'],
        defines         = 'G_LOG_DOMAIN="MIO"',
        uselib          = ['GTK', 'GLIB'],
        install_path    = None)  # do not install this library

    # Scintilla
    files = bld.srcnode.ant_glob('scintilla/**/*.cxx', src=True, dir=False)
    scintilla_sources.update(files)
    bld.new_task_gen(
        features        = ['c', 'cxx', 'cxxstlib'],
        name            = 'scintilla',
        target          = 'scintilla',
        source          = scintilla_sources,
        includes        = ['.', 'scintilla/include', 'scintilla/src', 'scintilla/lexlib'],
        uselib          = ['GTK', 'GLIB', 'GMODULE'],
        install_path    = None)  # do not install this library

    # Geany
    if bld.env['HAVE_VTE'] == 1:
        geany_sources.add('src/vte.c')
    if is_win32:
        geany_sources.add('src/win32.c')
        geany_sources.add('geany_private.rc')

    bld.new_task_gen(
        features        = ['c', 'cxx', 'cprogram'],
        name            = 'geany',
        target          = 'geany',
        source          = geany_sources,
        includes        = ['.', 'scintilla/include/', 'tagmanager/include/'],
        defines         = ['G_LOG_DOMAIN="Geany"', 'GEANY_PRIVATE'],
        linkflags       = [] if is_win32 else ['-Wl,--export-dynamic'],
        uselib          = ['GTK', 'GLIB', 'GMODULE', 'GIO', 'GTHREAD', 'WIN32', 'SUNOS_SOCKET'],
        use             = ['scintilla', 'tagmanager', 'mio'])

    # geanyfunctions.h
    bld.new_task_gen(
        source  = ['plugins/genapi.py', 'src/plugins.c'],
        name    = 'geanyfunctions.h',
        before  = ['c', 'cxx'],
        cwd     = '%s/plugins' % bld.path.abspath(),
        rule    = '%s genapi.py -q' % sys.executable)

    # Plugins
    if bld.env['HAVE_PLUGINS'] == 1:
        build_plugin('classbuilder')
        build_plugin('demoplugin', False)
        build_plugin('export')
        build_plugin('filebrowser')
        build_plugin('htmlchars')
        build_plugin('saveactions')
        build_plugin('splitwindow')

    # Translations
    if bld.env['INTLTOOL']:
        bld.new_task_gen(
            features        = ['linguas', 'intltool_po'],
            podir           = 'po',
            install_path    = '${LOCALEDIR}',
            appname         = 'geany')

    # geany.pc
    bld.new_task_gen(
        source          = 'geany.pc.in',
        dct             = {'VERSION': VERSION,
                           'DEPENDENCIES': 'gtk+-2.0 >= %s glib-2.0 >= %s' % \
                                (MINIMUM_GTK_VERSION, MINIMUM_GLIB_VERSION),
                           'prefix': bld.env['PREFIX'],
                           'exec_prefix': '${prefix}',
                           'libdir': '${exec_prefix}/lib',
                           'includedir': '${prefix}/include',
                           'datarootdir': '${prefix}/share',
                           'datadir': '${datarootdir}',
                           'localedir': '${datarootdir}/locale'})

    if not is_win32:
        # geany.desktop
        if bld.env['INTLTOOL']:
            bld.new_task_gen(
                features        = 'intltool_in',
                source          = 'geany.desktop.in',
                flags           = ['-d', '-q', '-u', '-c'],
                install_path    = '${DATADIR}/applications')

        # geany.1
        bld.new_task_gen(
            features        = 'subst',
            source          = 'doc/geany.1.in',
            target          = 'geany.1',
            dct             = {'VERSION': VERSION,
                                'GEANY_DATA_DIR': bld.env['DATADIR'] + '/geany'},
            install_path    = '${MANDIR}/man1')

        # geany.spec
        bld.new_task_gen(
            features        = 'subst',
            source          = 'geany.spec.in',
            target          = 'geany.spec',
            install_path    = None,
            dct             = {'VERSION': VERSION})

        # Doxyfile
        bld.new_task_gen(
            features        = 'subst',
            source          = 'doc/Doxyfile.in',
            target          = 'doc/Doxyfile',
            install_path    = None,
            dct             = {'VERSION': VERSION})

    ###
    # Install files
    ###
    if not is_win32:
        # Headers
        bld.install_files('${PREFIX}/include/geany', '''
            src/document.h src/editor.h src/encodings.h src/filetypes.h src/geany.h
            src/highlighting.h src/keybindings.h src/msgwindow.h src/plugindata.h
            src/prefs.h src/project.h src/search.h src/stash.h src/support.h
            src/templates.h src/toolbar.h src/ui_utils.h src/utils.h src/build.h
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
    for filename in 'AUTHORS ChangeLog COPYING README NEWS THANKS TODO'.split():
        basename = _uc_first(filename, bld)
        destination_filename = '%s%s' % (basename, ext)
        destination = os.path.join(base_dir, destination_filename)
        bld.install_as(destination, filename)

    start_dir = bld.path.find_dir('doc/images')
    bld.install_files('${DOCDIR}/%simages' % html_dir, start_dir.ant_glob('*.png'), cwd=start_dir)
    bld.install_as('${DOCDIR}/%s' % _uc_first('manual.txt', bld), 'doc/geany.txt')
    bld.install_as('${DOCDIR}/%s%s' % (html_dir, html_name), 'doc/geany.html')
    bld.install_as('${DOCDIR}/ScintillaLicense.txt', 'scintilla/License.txt')
    if is_win32:
        bld.install_as('${DOCDIR}/ReadMe.I18n.txt', 'README.I18N')
        bld.install_as('${DOCDIR}/Hacking.txt', 'HACKING')
    # Data
    data_dir = '' if is_win32 else 'geany'
    start_dir = bld.path.find_dir('data')
    bld.install_as('${DATADIR}/%s/GPL-2' % data_dir, 'COPYING')
    bld.install_files('${DATADIR}/%s' % data_dir, start_dir.ant_glob('filetype*'), cwd=start_dir)
    bld.install_files('${DATADIR}/%s' % data_dir, start_dir.ant_glob('*.tags'), cwd=start_dir)
    bld.install_files('${DATADIR}/%s' % data_dir, 'data/geany.glade')
    bld.install_files('${DATADIR}/%s' % data_dir, 'data/snippets.conf')
    bld.install_files('${DATADIR}/%s' % data_dir, 'data/ui_toolbar.xml')
    start_dir = bld.path.find_dir('data/colorschemes')
    template_dest = '${DATADIR}/%s/colorschemes' % data_dir
    bld.install_files(template_dest, start_dir.ant_glob('*'), cwd=start_dir)
    start_dir = bld.path.find_dir('data/templates')
    template_dest = '${DATADIR}/%s/templates' % data_dir
    bld.install_files(template_dest, start_dir.ant_glob('**/*'), cwd=start_dir, relative_trick=True)
    # Icons
    icon_dest = '${PREFIX}/share/icons' if is_win32 else '${DATADIR}/icons/hicolor/16x16/apps'
    start_dir = bld.path.find_dir('icons/16x16')
    bld.install_files(icon_dest, start_dir.ant_glob('*.png'), cwd=start_dir)
    if not is_win32:
        start_dir = bld.path.find_dir('icons/48x48')
        icon_dest = '${DATADIR}/icons/hicolor/48x48/apps'
        bld.install_files(icon_dest, start_dir.ant_glob('*.png'), cwd=start_dir)
        start_dir = bld.path.find_dir('icons/scalable')
        scalable_dest = '${DATADIR}/icons/hicolor/scalable/apps'
        bld.install_files(scalable_dest, start_dir.ant_glob('*.svg'), cwd=start_dir)


def distclean(ctx):
    Scripting.distclean(ctx)
    _remove_linguas_file()


def _remove_linguas_file():
    # remove LINGUAS file as well
    try:
        os.unlink(LINGUAS_FILE)
    except OSError:
        pass


@feature('linguas')
def write_linguas_file(self):
    if os.path.exists(LINGUAS_FILE):
        return
    linguas = ''
    if 'LINGUAS' in self.env:
        files = self.env['LINGUAS']
        for po_filename in files.split(' '):
            if os.path.exists('po/%s.po' % po_filename):
                linguas += '%s ' % po_filename
    else:
        files = os.listdir('%s/po' % self.path.abspath())
        files.sort()
        for filename in files:
            if filename.endswith('.po'):
                linguas += '%s ' % filename[:-3]
    file_h = open(LINGUAS_FILE, 'w')
    file_h.write('# This file is autogenerated. Do not edit.\n%s\n' % linguas)
    file_h.close()


def _post_install(ctx):
    is_win32 = _target_is_win32(ctx)
    if is_win32:
        return
    theme_dir = Utils.subst_vars('${DATADIR}/icons/hicolor', ctx.env)
    icon_cache_updated = False
    if not ctx.options.destdir:
        ctx.exec_command('gtk-update-icon-cache -q -f -t %s' % theme_dir)
        Logs.pprint('GREEN', 'GTK icon cache updated.')
        icon_cache_updated = True
    if not icon_cache_updated:
        Logs.pprint('YELLOW', 'Icon cache not updated. After install, run this:')
        Logs.pprint('YELLOW', 'gtk-update-icon-cache -q -f -t %s' % theme_dir)


def updatepo(ctx):
    """update the message catalogs for internationalization"""
    potfile = '%s.pot' % APPNAME
    os.chdir('%s/po' % top)
    try:
        try:
            old_size = os.stat(potfile).st_size
        except OSError:
            old_size = 0
        ctx.exec_command('intltool-update --pot -g %s' % APPNAME)
        size_new = os.stat(potfile).st_size
        if size_new != old_size:
            Logs.pprint('CYAN', 'Updated POT file.')
            Logs.pprint('CYAN', 'Updating translations')
            ret = ctx.exec_command('intltool-update -r -g %s' % APPNAME)
            if ret != 0:
                Logs.pprint('RED', 'Updating translations failed')
        else:
            Logs.pprint('CYAN', 'POT file is up to date.')
    except OSError:
        Logs.pprint('RED', 'Failed to generate pot file.')


def apidoc(ctx):
    """generate API reference documentation"""
    basedir = ctx.path.abspath()
    doxygen = _find_program(ctx, 'doxygen')
    doxyfile = '%s/%s/doc/Doxyfile' % (basedir, out)
    os.chdir('doc')
    Logs.pprint('CYAN', 'Generating API documentation')
    ret = ctx.exec_command('%s %s' % (doxygen, doxyfile))
    if ret != 0:
        raise WafError('Generating API documentation failed')
    # update hacking.html
    cmd = _find_rst2html(ctx)
    ctx.exec_command('%s  -stg --stylesheet=geany.css %s %s' % (cmd, '../HACKING', 'hacking.html'))
    os.chdir('..')


def htmldoc(ctx):
    """generate HTML documentation"""
    # first try rst2html.py as it is the upstream default, fall back to rst2html
    cmd = _find_rst2html(ctx)
    os.chdir('doc')
    Logs.pprint('CYAN', 'Generating HTML documentation')
    ctx.exec_command('%s  -stg --stylesheet=geany.css %s %s' % (cmd, 'geany.txt', 'geany.html'))
    os.chdir('..')


def _find_program(ctx, cmd, **kw):
    def noop(*args):
        pass

    ctx = ConfigurationContext()
    ctx.to_log = noop
    ctx.msg = noop
    return ctx.find_program(cmd, **kw)


def _find_rst2html(ctx):
    cmds = ['rst2html.py', 'rst2html']
    for command in cmds:
        cmd = _find_program(ctx, command, mandatory=False)
        if cmd:
            break
    if not cmd:
        raise WafError(
            'rst2html.py could not be found. Please install the Python docutils package.')
    return cmd


def _add_define_to_env(conf, key):
    value = conf.get_define(key)
    # strip quotes
    value = value.replace('"', '')
    conf.env[key] = value


def _add_to_env_and_define(conf, key, value, quote=False):
    conf.define(key, value, quote)
    conf.env[key] = value


def _define_from_opt(conf, define_name, opt_value, default_value, quote=1):
    value = default_value
    if opt_value:
        if isinstance(opt_value, bool):
            opt_value = 1
        value = opt_value

    if value is not None:
        _add_to_env_and_define(conf, define_name, value, quote)
    else:
        conf.undefine(define_name)


def _get_git_rev(conf):
    if not os.path.isdir('.git'):
        return

    try:
        cmd = 'git rev-parse --short --revs-only HEAD'
        revision = conf.cmd_and_log(cmd).strip()
    except WafError:
        return None
    else:
        return revision


def _load_intltool_if_available(conf):
    try:
        conf.check_tool('intltool')
        if 'LINGUAS' in os.environ:
            conf.env['LINGUAS'] = os.environ['LINGUAS']
    except WafError:
        # on Windows, we don't hard depend on intltool, on all other platforms raise an error
        if not _target_is_win32(conf):
            raise


def _target_is_win32(ctx):
    if 'is_win32' in ctx.env:
        # cached
        return ctx.env['is_win32']
    is_win32 = None
    if sys.platform == 'win32':
        is_win32 = True
    if is_win32 is None:
        if ctx.env and 'CC' in ctx.env:
            env_cc = ctx.env['CC']
            if not isinstance(env_cc, str):
                env_cc = ''.join(env_cc)
            is_win32 = (env_cc.find('mingw') != -1)
    if is_win32 is None:
        is_win32 = False
    # cache for future checks
    ctx.env['is_win32'] = is_win32
    return is_win32


def _uc_first(string, ctx):
    if _target_is_win32(ctx):
        return string.title()
    return string
