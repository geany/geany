#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Author:  Enrico Tröger
# License: GPL v2 or later
#
# (based on the script at http://svn.python.org/view/*checkout*/python/trunk/Tools/scripts/ptags.py)
#
# This script should be run in the top source directory.
#
# Parses all files given on command line for Python classes or functions and write
# them into data/python.tags (internal tagmanager format).
# If called without command line arguments, a preset of common Python libs is used.

import datetime
import imp
import inspect
import os
import re
import sys
import types

PYTHON_LIB_DIRECTORY = os.path.dirname(os.__file__)
PYTHON_LIB_IGNORE_PACKAGES = (u'test', u'dist-packages', u'site-packages')
# multiprocessing.util registers an atexit function which breaks this script on exit
PYTHON_LIB_IGNORE_MODULES = (u'multiprocessing/util.py',)

# (from tagmanager/tm_tag.c:32)
TA_NAME = '%c' % 200,
TA_TYPE = '%c' % 204
TA_ARGLIST = '%c' % 205
TA_SCOPE = '%c' % 206

# TMTagType (tagmanager/tm_tag.h:47)
TYPE_CLASS = '%d' % 1
TYPE_FUNCTION = '%d' % 128

tag_filename = 'data/python.tags'
tag_regexp = '^[ \t]*(def|class)[ \t]+([a-zA-Z0-9_]+)[ \t]*(\(.*\))[:]'


########################################################################
class Parser:

    #----------------------------------------------------------------------
    def __init__(self):
        self.tags = {}
        self.re_matcher = re.compile(tag_regexp)

    #----------------------------------------------------------------------
    def _get_superclass(self, _object):
        """
        Python class base-finder
        (found on http://mail.python.org/pipermail/python-list/2002-November/173949.html)

        @param _object (object)
        @return superclass (object)
        """
        try:
            #~ TODO print inspect.getmro(c)
            if type(_object) == types.ClassType:
                return _object.__bases__[0].__name__
            else:
                return _object.__mro__[1].__name__
        except IndexError:
            return ''

    #----------------------------------------------------------------------
    def _formatargspec(self, args, varargs=None, varkw=None, defaults=None,
                      formatarg=str,
                      formatvarargs=lambda name: '*' + name,
                      formatvarkw=lambda name: '**' + name,
                      formatvalue=lambda value: '=' + repr(value),
                      join=inspect.joinseq):
        """Format an argument spec from the 4 values returned by getargspec.

        The first four arguments are (args, varargs, varkw, defaults).  The
        other four arguments are the corresponding optional formatting functions
        that are called to turn names and values into strings.  The ninth
        argument is an optional function to format the sequence of arguments."""
        specs = []
        if defaults:
            firstdefault = len(args) - len(defaults)
        for i in range(len(args)):
            spec = inspect.strseq(args[i], formatarg, join)
            if defaults and i >= firstdefault:
                d = defaults[i - firstdefault]
                # this is the difference from the original formatargspec() function
                # to use nicer names then the default repr() output
                if hasattr(d, '__name__'):
                    d = d.__name__
                spec = spec + formatvalue(d)
            specs.append(spec)
        if varargs is not None:
            specs.append(formatvarargs(varargs))
        if varkw is not None:
            specs.append(formatvarkw(varkw))
        return ', '.join(specs)

    #----------------------------------------------------------------------
    def _add_tag(self, obj, tag_type, parent=''):
        """
        Verify the found tag name and if it is valid, add it to the list

        @param obj (instance)
        @param tag_type (str)
        @param parent (str)
        """
        args = ''
        scope = ''
        try:
            args = apply(self._formatargspec, inspect.getargspec(obj))
        except (TypeError, KeyError):
            pass
        if parent:
            if tag_type == TYPE_CLASS:
                args = '(%s)' % parent
            else:
                scope = '%s%s' % (TA_SCOPE, parent)
        tagname = obj.__name__
        # check for duplicates
        if len(tagname) < 4:
            # skip short tags
            return
        tag = '%s%s%s%s%s%s\n' % (tagname, TA_TYPE, tag_type, TA_ARGLIST, args, scope)
        if not tagname in self.tags:
            self.tags[tagname] = tag

    #----------------------------------------------------------------------
    def process_file(self, filename):
        """
        Read the file specified by filename and look for class and function definitions

        @param filename (str)
        """
        try:
            module = imp.load_source('tags_file_module', filename)
        except IOError, e:
            # file not found
            print '%s: %s' % (filename, e)
            return
        except Exception:
            module = None

        if module:
            symbols = inspect.getmembers(module, callable)
            for obj_name, obj in symbols:
                try:
                    name = obj.__name__
                except AttributeError:
                    name = obj_name
                if not name or not isinstance(name, basestring) or is_private_identifier(name):
                    # skip non-public tags
                    continue
                if inspect.isfunction(obj):
                    self._add_tag(obj, TYPE_FUNCTION)
                elif inspect.isclass(obj):
                    self._add_tag(obj, TYPE_CLASS, self._get_superclass(obj))
                    try:
                        methods = inspect.getmembers(obj, inspect.ismethod)
                    except (TypeError, AttributeError):
                        methods = []
                    for m_name, m_obj in methods:
                        # skip non-public tags
                        if is_private_identifier(m_name) or not inspect.ismethod(m_obj):
                            continue
                        self._add_tag(m_obj, TYPE_FUNCTION, name)
        else:
            # plain regular expression based parsing
            filep = open(filename)
            for line in filep:
                m = self.re_matcher.match(line)
                if m:
                    tag_type_str, tagname, args = m.groups()
                    if not tagname or is_private_identifier(tagname):
                        # skip non-public tags
                        continue
                    if tag_type_str == 'class':
                        tag_type = TYPE_CLASS
                    else:
                        tag_type = TYPE_FUNCTION
                    args = args.strip()
                    tag = '%s%s%s%s%s\n' % (tagname, TA_TYPE, tag_type, TA_ARGLIST, args)
                    if not tagname in self.tags:
                        self.tags[tagname] = tag
            filep.close()

    #----------------------------------------------------------------------
    def write_to_file(self, filename):
        """
        Sort the found tags and write them into the file specified by filename

        @param filename (str)
        """
        result = self.tags.values()
        # sort the tags
        result.sort()
        # write them
        target_file = open(filename, 'wb')
        target_file.write(
            '# format=tagmanager - Automatically generated file - do not edit (created on %s)\n' % \
            datetime.datetime.now().ctime())
        for symbol in result:
            if not symbol == '\n': # skip empty lines
                target_file.write(symbol)
        target_file.close()


#----------------------------------------------------------------------
def is_private_identifier(tagname):
    return tagname.startswith('_') or tagname.endswith('_')


#----------------------------------------------------------------------
def get_module_filenames(path):
    def ignore_package(package):
        for ignore in PYTHON_LIB_IGNORE_PACKAGES:
            if ignore in package:
                return True
        return False

    # the loop is quite slow but it doesn't matter for this script
    filenames = list()
    python_lib_directory_len = len(PYTHON_LIB_DIRECTORY)
    for base, dirs, files in os.walk(path):
        package = base[(python_lib_directory_len + 1):]
        if ignore_package(package):
            continue
        for filename in files:
            module_name = os.path.join(package, filename)
            if module_name in PYTHON_LIB_IGNORE_MODULES:
                continue
            if filename.endswith('.py'):
                module_filename = os.path.join(base, filename)
                filenames.append(module_filename)
    return filenames


#----------------------------------------------------------------------
def main():
    # process files given on command line
    args = sys.argv[1:]
    if not args:
        args = get_module_filenames(PYTHON_LIB_DIRECTORY)

    parser = Parser()

    for filename in args:
        parser.process_file(filename)

    parser.write_to_file(tag_filename)


if __name__ == '__main__':
    main()

