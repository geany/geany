#!/usr/bin/env python3

import glob
import os
import shutil
import sys

themes = [
    # Public domain
    'evg-ega-dark.conf',
    'sleepy-pastel.conf',

    # MIT
    'earthsong.conf',
    'one-dark.conf',
    'solarized-dark.conf',
    'solarized-light.conf',

    # 2-clause BSD
    'kugel.conf',
    'pygments.conf',

    # GPL 2 or later
    'black.conf',
    'darcula.conf',
    'dark-colors.conf',
    'dark.conf',
    'dark-fruit-salad.conf',
    'delt-dark.conf',
    'grey8.conf',
    'hacker.conf',
    'mc.conf',
    'metallic-bottle.conf',
    'notepad-plus-plus.conf',
    'oblivion2.conf',
    'retro.conf',
    'spyder-dark.conf',
    'steampunk.conf',
    'tango-light.conf',
    'ubuntu.conf',
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
    # GPL 3 or later
    'epsilon.conf',

    # no explicit license
    'fluffy.conf',
    'kary-pro-colors-dark.conf',
    'kary-pro-colors-light.conf',
    'kurayami.conf',  # original license MIT but no license of the Geany variant
    'monokai.conf',
    'railcasts2.conf',
    'tango-dark.conf',
    'zenburn.conf',
]

usage_msg = 'Usage: update-themes.py <geany-themes/colorschemes> <geany/data/colorschemes directory>'

if len(sys.argv) != 3:
    print(usage_msg)
    sys.exit(1)

srcdir = os.path.abspath(sys.argv[1])
dstdir = os.path.abspath(sys.argv[2])

# some sanity checks
if (not os.path.isdir(srcdir) or not os.path.isdir(dstdir) or
    not os.path.isfile(srcdir + '/dark.conf') or not dstdir.endswith('data/colorschemes')):
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
    print("Don't forget to add the newly added themes to data/Makefile.am !!!")
