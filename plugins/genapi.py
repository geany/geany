#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#       genapi.py - this file is part of Geany, a fast and lightweight IDE
#
#       Copyright 2008-2011 Nick Treleaven <nick.treleaven<at>btinternet.com>
#       Copyright 2008-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
#
#       This program is free software; you can redistribute it and/or modify
#       it under the terms of the GNU General Public License as published by
#       the Free Software Foundation; either version 2 of the License, or
#       (at your option) any later version.
#
#       This program is distributed in the hope that it will be useful,
#       but WITHOUT ANY WARRANTY; without even the implied warranty of
#       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#       GNU General Public License for more details.
#
#       You should have received a copy of the GNU General Public License
#       along with this program; if not, write to the Free Software
#       Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# $(Id)

r"""
Creates macros for each plugin API function pointer, e.g.:

#define plugin_add_toolbar_item \
    geany_functions->p_plugin->plugin_add_toolbar_item
"""


import re
import sys


def get_function_names():
    names = []
    filep = open('../src/plugins.c')
    while 1:
        line = filep.readline()
        if line == "":
            break
        match = re.match("^\t&([a-z][a-z0-9_]+)", line)
        if match:
            symbol = match.group(1)
            if not symbol.endswith('_funcs'):
                names.append(symbol)
    filep.close()
    return names


def get_api_tuple(source):
    match = re.match("^([a-z]+)_([a-z][a-z0-9_]+)$", source)
    return 'p_' + match.group(1), match.group(2)


header = \
r'''/* This file is generated automatically by genapi.py - do not edit. */

/** @file %s @ref geany_functions wrappers.
 * This allows the use of normal API function names in plugins by defining macros.
 *
 * E.g.:@code
 * #define plugin_add_toolbar_item \
 * 	geany_functions->p_plugin->plugin_add_toolbar_item @endcode
 *
 * You need to declare the @ref geany_functions symbol yourself.
 *
 * Note: This must be included after all other API headers to prevent conflicts with
 * other header's function prototypes - this is done for you when using geanyplugin.h.
 */

#ifndef GEANY_FUNCTIONS_H
#define GEANY_FUNCTIONS_H

'''

if __name__ == "__main__":
    outfile = 'geanyfunctions.h'

    fnames = get_function_names()
    if not fnames:
        sys.exit("No function names read!")

    f = open(outfile, 'w')
    f.write(header % (outfile))

    for fname in fnames:
        ptr, name = get_api_tuple(fname)
        # note: name no longer needed
        f.write('#define %s \\\n\tgeany_functions->%s->%s\n' % (fname, ptr, fname))

    f.write('\n#endif\n')
    f.close()

    if not '-q' in sys.argv:
        sys.stdout.write('Generated %s\n' % outfile)
