#!/usr/bin/env python
#
#  Copyright 2015-2016 Thomas Martitz <kugel@rockbox.org>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.

import sys
import os
from collections import OrderedDict
from pycparser import c_parser, c_ast, parse_file
from optparse import OptionParser


# this function was taken from the cdecl.py example from the upstream pycparser project
# and modified to suit this script
def _explain_type(decl):
    """ Recursively explains a type decl node
    """
    typ = type(decl)

    if typ == c_ast.TypeDecl:
        quals = ' '.join(decl.quals) + ' ' if decl.quals else ''
        return quals + _explain_type(decl.type)
    elif typ == c_ast.Typename or typ == c_ast.Decl:
        return _explain_type(decl.type)
    elif typ == c_ast.IdentifierType:
        return ' '.join(decl.names)
    elif typ == c_ast.Struct:
        return 'struct ' + decl.name
    elif typ == c_ast.PtrDecl:
        quals = ' '.join(decl.quals) + ' ' if decl.quals else ''
        return quals + _explain_type(decl.type) + ' *'
    elif typ == c_ast.ArrayDecl:
        arr = '[]'
        if decl.dim:
            arr = '[%s]' % decl.dim.value
        return _explain_type(decl.type) + arr

    elif typ == c_ast.FuncDecl:
        if decl.args:
            params = [_explain_type(param) for param in decl.args.params]
            args = ', '.join(params)
        else:
            args = ''

        return ("%s %s(%s)" % (_explain_type(decl.type), decl.name, args))


class Function(object):
    def __init__(self, decl):
        self.decl = decl

    def name(self):
        return self.decl.name

    def new_name(self):
        return self.decl.name.replace("sci_", "scintilla_object_", 1)

    def type(self):
        return _explain_type(self.decl.type.type)

    def params(self):
        d = OrderedDict()
        for param in self.decl.type.args.params:
            d[param.name] = _explain_type(param)
        return d

    def declaration(self):
        params = []
        for k, v in self.params().items():
            params += [v + " " + k]
        if (len(params) == 0):
            return "%s %s(void)" %  (self.type(), self.new_name())
        return "%s %s(%s)" % ( self.type(), self.new_name(), ', '.join(params) )

    def wrapper(self):
        call = ""
        if (self.type() != "void"):
            call += "return "
        call += self.name()
        call += "(%s)" % ', '.join(self.params().keys())
        return "{\n\t%s;\n}" % call

# A simple visitor for FuncDef nodes that prints the names and
# locations of function definitions.
class FuncDefVisitor(c_ast.NodeVisitor):
    def __init__(self):
        self.funcs = []

    def visit_FuncDef(self, node):
        if ("extern" in node.decl.storage):
            func = Function(node.decl)
            self.funcs += [func]

    def print_funcs(self, output=sys.stdout):
        for f in self.funcs:
            output.write("\nGEANY_API_SYMBOL\n" + f.declaration() + "\n" + f.wrapper() + "\n\n")

def main(args):
    outfile = None
    
    parser = OptionParser(usage="usage: %prog [options] INPUT")
    parser.add_option("-o", "--output", metavar="FILE", help="Write output to FILE",
                      action="store", dest="outfile")
    opts, args = parser.parse_args(args[1:])

    source_file = args[0]
    if not (os.path.exists(source_file)):
        sys.stderr.write("invalid input file (pass path to sciwrappers.c)\n")
        return 1

    ast = parse_file(source_file, use_cpp=True, cpp_args=r'-DPYCPARSER')
    v = FuncDefVisitor()
    v.visit(ast)

    if (opts.outfile):
        try:
            outfile = open(opts.outfile, "w+")
        except OSError as err:
            sys.stderr.write("failed to open \"%s\" for writing (%s)\n" % (opts.outfile, err.strerror))
            return 1
    else:
        outfile = sys.stdout
    
    outfile.write("/*\n * Automatically generated file - do not edit\n */\n\n")
    outfile.write("#include \"sciwrappers.h\"\n")
    v.print_funcs(outfile)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
