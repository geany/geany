#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#       genapi.py - this file is part of Geany, a fast and lightweight IDE
#
#       Copyright 2008 Nick Treleaven <nick.treleaven<at>btinternet.com>
#       Copyright 2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
	p_plugin->add_toolbar_item
"""


import re, sys

def get_function_names():
	names = []
	try:
		f = open('../src/plugins.c')
		while 1:
			l = f.readline()
			if l == "":
				break;
			m = re.match("^\t&([a-z][a-z0-9_]+)", l)
			if m:
				s = m.group(1)
				if not s.endswith('_funcs'):
					names.append(s)
		f.close
	except:
		pass
	return names

def get_api_tuple(str):
	m = re.match("^([a-z]+)_([a-z][a-z0-9_]+)$", str)
	return 'p_' + m.group(1), m.group(2)


if __name__ == "__main__":
	outfile = 'geanyfunctions.h'

	fnames = get_function_names()
	if not fnames:
		sys.exit("No function names read!")

	f = open(outfile, 'w')
	print >>f, '/* This file is generated automatically by genapi.py - do not edit.\n *\n' +\
		' * @file %s @ref geany_functions wrappers.\n' % (outfile) +\
		' * This allows the use of normal API function names in plugins.\n' +\
		' * You need to declare the @ref geany_functions symbol yourself.\n */\n'
	print >>f, '#ifndef GEANY_FUNCTIONS_H'
	print >>f, '#define GEANY_FUNCTIONS_H\n'
	for fname in fnames:
		ptr, name = get_api_tuple(fname)
		print >>f, '#define %s \\\n\tgeany_functions->%s->%s' % (fname, ptr, name)
	print >>f, '\n#endif'
	f.close

	if not '-q' in sys.argv:
		print 'Generated ' + outfile
