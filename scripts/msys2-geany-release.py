#!/usr/bin/env python3

import argparse
import glob
import os
import shutil
from os.path import exists, isfile, join
from subprocess import check_call

"""
This script prepares a Geany release installer on Windows.
The following steps will be executed:
- strip binary files (geany.exe, plugin .dlls)
- create installer
"""

if 'GITHUB_WORKSPACE' in os.environ:
    if os.environ['GITHUB_REPOSITORY'].endswith("/geany"):
        SOURCE_DIR = os.environ['GITHUB_WORKSPACE']
    else:
        print(f"GITHUB_REPOSITORY={os.environ['GITHUB_REPOSITORY']}")
        SOURCE_DIR = join(os.environ['GITHUB_WORKSPACE'], ".geany_source")
    BASE_DIR = join(os.environ['GITHUB_WORKSPACE'], 'geany_build')
else:
    # adjust paths to your needs ($HOME is used because expanduser() returns the Windows home directory)
    SOURCE_DIR = join(os.environ['HOME'], 'git', 'geany')
    BASE_DIR = join(os.environ['HOME'], 'geany_build')
BUILD_DIR = join(SOURCE_DIR, '_build')
GEANY_THEMES_DIR = join(SOURCE_DIR, 'data')
RELEASE_DIR_ORIG = join(BASE_DIR, 'release', 'geany-orig')
RELEASE_DIR = join(BASE_DIR, 'release', 'geany')
BUNDLE_BASE_DIR = join(BASE_DIR, 'bundle')
BUNDLE_GTK = join(BASE_DIR, 'bundle', 'geany-gtk')


def run_command(*cmd, **kwargs):
    print('Execute command: {}'.format(' '.join(cmd)))
    check_call(cmd, **kwargs)


def prepare_release_dir():
    os.makedirs(RELEASE_DIR_ORIG, exist_ok=True)
    if exists(RELEASE_DIR):
        shutil.rmtree(RELEASE_DIR)
    shutil.copytree(RELEASE_DIR_ORIG, RELEASE_DIR, symlinks=True, ignore=None)


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


def make_release(version_number):
    # copy the release dir as it gets modified implicitly by converting files,
    # we want to keep a pristine version before we start
    prepare_release_dir()

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
    INSTALLER_NAME = join(BASE_DIR, f'geany-{version_number}_setup.exe')
    run_command(
        'makensis',
        '/WX',
        '/V3',
        f'/DGEANY_RELEASE_DIR={RELEASE_DIR}',
        f'/DGEANY_THEMES_DIR={GEANY_THEMES_DIR}',
        f'/DGTK_BUNDLE_DIR={BUNDLE_GTK}',
        f'-DGEANY_INSTALLER_NAME={INSTALLER_NAME}',
        f'{SOURCE_DIR}/geany.nsi')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="This script prepares a Geany release installer on Windows")
    parser.add_argument("version_number", help="Version Number (e.g. 2.1)")
    opts = parser.parse_args()
    make_release(opts.version_number)
