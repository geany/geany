#!/usr/bin/env python3

import glob
import os
import shutil
from os.path import exists, isfile, join
from subprocess import check_call

"""
This script prepares a Geany release on Windows.
The following steps will be executed:
- update geany-themes repository
- strip binary files (geany.exe, plugin .dlls)
- sign binary files with certificate
- create installers
- sign installers
"""

VERSION = '2.1'
# adjust paths to your needs ($HOME is used because expanduser() returns the Windows home directory)
BASE_DIR = join(os.environ['HOME'], 'geany_build')
SOURCE_DIR = join(os.environ['HOME'], 'git', 'geany')
BUILD_DIR = join(os.environ['HOME'], 'git', 'geany', '_build')
RELEASE_DIR_ORIG = join(BASE_DIR, 'release', 'geany-orig')
RELEASE_DIR = join(BASE_DIR, 'release', 'geany')
BUNDLE_BASE_DIR = join(BASE_DIR, 'bundle')
BUNDLE_GTK = join(BASE_DIR, 'bundle', 'geany-gtk')
GEANY_THEMES_URL = 'https://github.com/geany/geany-themes/archive/refs/heads/master.zip'
GEANY_THEMES_DIR = join(BUNDLE_BASE_DIR, 'geany-themes')
INSTALLER_NAME = join(BASE_DIR, f'geany-{VERSION}_setup.exe')


def run_command(*cmd, **kwargs):
    print('Execute command: {}'.format(' '.join(cmd)))
    check_call(cmd, **kwargs)


def prepare_release_dir():
    os.makedirs(RELEASE_DIR_ORIG, exist_ok=True)
    if exists(RELEASE_DIR):
        shutil.rmtree(RELEASE_DIR)
    shutil.copytree(RELEASE_DIR_ORIG, RELEASE_DIR, symlinks=True, ignore=None)


def download_geany_themes():
    if exists(GEANY_THEMES_DIR):
        shutil.rmtree(GEANY_THEMES_DIR)
    os.makedirs(GEANY_THEMES_DIR, exist_ok=True)
    run_command('wget', '-O', 'geany-themes-master.zip', GEANY_THEMES_URL, cwd=BASE_DIR)
    run_command('unzip', '-d', GEANY_THEMES_DIR, 'geany-themes-master.zip', cwd=BASE_DIR)
    os.unlink(join(BASE_DIR, 'geany-themes-master.zip'))


def convert_text_files(*paths):
    for item in paths:
        files = glob.glob(item)
        for filename in files:
            if isfile(filename):
                run_command('unix2dos', '--quiet', filename)


def strip_files(*paths):
    for item in paths:
        files = glob.glob(item)
        for filename in files:
            run_command('strip', filename)


def make_release():
    # copy the release dir as it gets modified implicitly by signing and converting files,
    # we want to keep a pristine version before we start
    prepare_release_dir()
    # update Geany Themes repo
    download_geany_themes()

    binary_files = (
        f'{RELEASE_DIR}/bin/geany.exe',
        f'{RELEASE_DIR}/bin/*.dll',
        f'{RELEASE_DIR}/lib/geany/*.dll',
        f'{RELEASE_DIR}/lib/*.dll')
    # strip binaries
    strip_files(*binary_files)
    # unix2dos conversion
    text_files = (
        f'{RELEASE_DIR}/*.txt',
        f'{RELEASE_DIR}/share/doc/geany/*',)
    convert_text_files(*text_files)
    # create installer
    shutil.copy(join(BUILD_DIR, 'geany.nsi'), SOURCE_DIR)
    run_command(
        'makensis',
        '/WX',
        '/V3',
        f'/DGEANY_RELEASE_DIR={RELEASE_DIR}',
        f'/DGEANY_THEMES_DIR={GEANY_THEMES_DIR}/geany-themes-master',
        f'/DGTK_BUNDLE_DIR={BUNDLE_GTK}',
        f'-DGEANY_INSTALLER_NAME={INSTALLER_NAME}',
        f'{SOURCE_DIR}/geany.nsi')


if __name__ == '__main__':
    make_release()
