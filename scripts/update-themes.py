#!/usr/bin/env python3

import glob
import os
import shutil
import sys

themes = [
    # Public domain
    'evg-ega-dark.conf',
    'sleepy-pastel.conf',
    'tango-dark.conf',

    # MIT
    'earthsong.conf',
    'kurayami.conf',
    'one-dark.conf',
    'solarized-dark.conf',
    'solarized-light.conf',

    # 2-clause BSD
    'kugel.conf',
    'pygments.conf',

    # GPL 2 or later
    'alt.conf',
    'black.conf',
    'carbonfox.conf',
    'darcula.conf',
    'dark-colors.conf',
    'dark.conf',
    'dark-fruit-salad.conf',
    'delt-dark.conf',
    'epsilon.conf',
    'grey8.conf',
    'gruvbox-dark.conf',
    'hacker.conf',
    'mc.conf',
    'metallic-bottle.conf',
    'notepad-plus-plus.conf',
    'oblivion2.conf',
    'octagon.conf',
    'retro.conf',
    'spyder-dark.conf',
    'steampunk.conf',
    'tango-light.conf',
    'ubuntu.conf',
    'underthesea.conf',
    'vibrant-ink.conf',

    # LGPL 2 or later
    'abc-dark.conf',
    'abc-light.conf',
    'bespin.conf',
    'cyber-sugar.conf',
    'github.conf',
    'himbeere.conf',
    'inkpot.conf',
    'matcha.conf',
    'slushpoppies.conf',
    'tinge.conf',

    # LGPL 2.1 or later
    'gedit.conf',
]

ignored = [
    # converted to Geany from https://github.com/mig/gedit-themes which does
    # not mention any license or authors of individual themes
    'fluffy.conf',
    'railcasts2.conf',
    'zenburn.conf',

    # exact license not yet clarified
    'kary-pro-colors-dark.conf',
    'kary-pro-colors-light.conf',
    'monokai.conf',
]

usage_msg = 'Usage: update-themes.py <geany-themes/colorschemes> <geany/data/colorschemes>'

if len(sys.argv) != 3:
    print(usage_msg)
    sys.exit(1)

srcdir = os.path.abspath(sys.argv[1])
dstdir = os.path.abspath(sys.argv[2])

# some sanity checks
if (not os.path.isdir(srcdir) or not os.path.isdir(dstdir) or
    not os.path.isfile(srcdir + '/alt.conf') or not dstdir.endswith('data/colorschemes')):
    print(usage_msg)
    sys.exit(1)

os.chdir(srcdir)

new_theme = False
source_themes = glob.glob('*.conf')

for theme in source_themes:
    if theme in ignored:
        continue
    if theme not in themes:
        print(f'Theme {theme} not listed in update-themes.py - update it and re-run agagin')
        continue
    if not os.path.isfile(dstdir + f'/{theme}'):
        new_theme = True
        print(f'New theme: {theme}')
    shutil.copy(theme, dstdir)

if new_theme:
    print("\nDon't forget to add the newly added themes to data/Makefile.am !!!")

os.chdir(dstdir)
dst_themes = glob.glob('*.conf')

extra_dst_themes = set(dst_themes) - set(source_themes)
if extra_dst_themes:
    print((f'\nWarning: themes {extra_dst_themes} found in Geany themes directory but not in geany-themes. '
           f'Should these be added to geany-themes?'))
