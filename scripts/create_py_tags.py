#!/usr/bin/env python3
#
# Author:  Enrico Tröger
# License: GPL v2 or later
#
# (based on the script at http://svn.python.org/view/*checkout*/python/trunk/Tools/scripts/ptags.py)
#
# This script should be run in the top source directory.
#
# Parses all files in the directories given on command line for Python classes or functions and
# write them into data/tags/std.py.tags (ctags format).
# If called without command line arguments, a preset of common Python libs is used.
#
# WARNING
# Be aware that running this script will actually *import* all modules given on the command line
# or in the standard library path of your Python installation. Dependent on what Python modules
# you have installed, this might not be what you want and can have weird side effects.
# You have been warned.
#
# It should be however relatively safe to execute this script from a fresh Python installation
# installed into a dedicated prefix or from an empty virtualenv or ideally in a Docker container
# in the Geany project directory:
# docker run --rm -it --user $(id -u):$(id -g) -v $(pwd):/data --workdir /data python:3.11-alpine python scripts/create_py_tags.py
#

import importlib.util
import inspect
import os
import re
import sys
import sysconfig
import warnings
from pathlib import Path

from create_tags_helper import format_tag, write_ctags_file

# treat all DeprecationWarnings as errors so we can catch them to ignore the corresponding modules
warnings.filterwarnings('error', category=DeprecationWarning)

PYTHON_LIB_DIRECTORY = Path(os.__file__).parent
PYTHON_LIB_IGNORE_PACKAGES = ['dist-packages', 'distutils', 'encodings', 'idlelib', 'lib2to3',
                              'site-packages', 'test', 'turtledemo', 'Tools']
# some modules/classes are deprecated or execute funky code when they are imported
# which we really don't want here (though if you feel funny, try: 'import antigravity')
PYTHON_LIB_IGNORE_MODULES = ('__phello__.foo', 'antigravity', 'asyncio.windows_events',
                             'asyncio.windows_utils', 'ctypes.wintypes', 'ensurepip._bundled',
                             'lib2to3', 'multiprocessing.popen_spawn_win32', 'this', 'turtle')
PYTHON_LIB_IGNORE_CLASSES = ('typing.io', 'typing.re')

# Python kinds
KIND_CLASS = 'class'
KIND_FUNCTION = 'function'
KIND_MEMBER = 'member'

TAG_FILENAME = 'data/tags/std.py.tags'
TAG_REGEXP = re.compile(r'^[ \t]*(def|class)[ \t]+([a-zA-Z0-9_]+)[ \t]*(\(.*\))[:]')
OBJECT_MEMORY_ADDRESS_REGEXP = re.compile(r'<(.+?) at 0x[0-9a-f]+(?:.+)>', flags=re.IGNORECASE)

# pylint: disable=no-else-return,no-self-use


class Parser:

    def __init__(self):
        self.tags = {}

    def _add_tag(self, object_name, object_, kind, module_path=None, parent=''):
        """
        Verify the found tag name and if it is valid, add it to the list

        @param object_ (instance)
        @param tag_type (str)
        @param parent (str)
        """
        if len(object_name) < 4 or is_private_identifier(object_name):
            return  # skip short and private tags
        if object_ is not None and not is_relevant_identifier(object_):
            return

        tag_key = (module_path, parent, object_name)
        if tag_key not in self.tags:
            signature = self._create_signature(object_) if object_ is not None else None
            self.tags[tag_key] = format_tag(object_name, kind, signature, parent)

    def _get_safe_parameter_default_value(self, value):
        """
        Replace possibly sensitive or just much information from the default value
        """
        # prevent evaluating of `os.environ` in cgi.print_environ(environ=os.environ) which
        # would lead to include the current full environment variables to be included
        # in the tags file
        if isinstance(value, (dict, os._Environ)) and value:  # pylint: disable=protected-access
            return f'<default-value-stripped {type(value)}>'
        if isinstance(value, str):
            # remove interpreter paths
            if sys.executable in value:
                return '/nonexistent/bin/python3'
            # remove interpreter paths
            if sys.prefix in value:
                return '/nonexistent'

        # for all other default values, return the string representation,
        # assuming it is shorter than repr()
        value_str = str(value)

        # remove object hex addresses, e.g
        # subTest(self, msg='<object object at 0x7f14bdfcd5a0>', **params)
        if OBJECT_MEMORY_ADDRESS_REGEXP.search(value_str):
            return OBJECT_MEMORY_ADDRESS_REGEXP.sub(r'<\1>', value_str)

        return value_str

    def _stringify_parameter_default_if_necessary(self, parameter):
        """
        Replace default values of the parameters with their string variants if they are not
        basic types. This is to avoid signatures like (`ssl.SSLContext.load_default_certs`):
        create_default_contextÌ128Í(purpose=<Purpose.SERVER_AUTH: _ASN1Object(nid=129, shortname='serverAuth', longname='TLS Web Server Authentication', oid='1.3.6.1.5.5.7.3.1')>, *, cafile=None, capath=None, cadata=None)ÎSSLContext  # noqa pylint: disable=line-too-long
        and create instead:
        create_default_contextÌ128Í(purpose='Purpose.SERVER_AUTH', *, cafile=None, capath=None, cadata=None)

        This is not perfect as it might suggest that the `purpose` parameter accepts a string.
        But having the full `repr()` result is even worse.
        """
        if not parameter.default or parameter.default is parameter.empty:
            return parameter
        if isinstance(parameter.default, (bool, int, float)):
            return parameter

        new_default = self._get_safe_parameter_default_value(parameter.default)
        return parameter.replace(default=new_default)

    def _create_signature(self, object_):
        """
        Create signature for the given `object_`.
        """
        try:
            signature = inspect.signature(object_)
        except (ValueError, TypeError):
            # inspect.signature() throws ValueError and TypeError for unsupported callables,
            # so we need to ignore the signature for this callable
            return ''

        new_parameters = []
        for parameter_name in signature.parameters:
            parameter = signature.parameters[parameter_name]
            if parameter.default and not isinstance(parameter.default, parameter.empty):
                new_parameter = self._stringify_parameter_default_if_necessary(parameter)
                new_parameters.append(new_parameter)
            else:
                new_parameters.append(parameter)

        return signature.replace(parameters=new_parameters)

    def process_module(self, module_path, module_filename):
        """
        Import the given module path and look for class and function definitions
        """
        module = None
        symbols = None
        module_error = None

        if module_path.endswith('__main__'):
            return  # ignore any executable modules, importing them would execute the module

        try:
            module = importlib.import_module(module_path)
        except DeprecationWarning as exc:
            print(f'Ignoring deprecated module "{module_path}" (reason: {exc})')
            return
        except Exception as exc:
            module_error = str(exc)
        else:
            symbols = inspect.getmembers(module)

        if symbols:
            self._process_module_with_inspect(symbols, module_path)
        else:
            # If error is empty, there are probably just no symbols in the module, e.g. on empty
            # __init__.py files. Try to parse them anyway. But log module_errors.
            if module_error:
                print(f'Using fallback parser for: {module_path} ({module_filename}, reason: {module_error})')

            self._process_module_with_fallback_parser(module_filename)

    def _process_module_with_inspect(self, symbols, module_path):
        """
        Try to analyse all symbols in the module as found by `inspect.getmembers`.
        """
        for obj_name, obj in symbols:
            if is_import(obj, module_path):
                continue

            # function and similar callables
            if inspect.isroutine(obj):
                self._add_tag(obj_name, obj, KIND_FUNCTION, module_path)
            # class
            elif inspect.isclass(obj):
                if _ignore_class(module_path, obj_name):
                    continue
                self._add_tag(obj_name, obj, KIND_CLASS, module_path)
                methods = inspect.getmembers(obj)
                # methods
                for m_name, m_obj in methods:
                    self._add_tag(m_name, m_obj, KIND_MEMBER, module_path, parent=obj_name)

    def _process_module_with_fallback_parser(self, module_filename):
        """
        Plain regular expression based parsing, used as fallback if `inspect`'ing the module is not possible
        """
        with open(module_filename, encoding='utf-8') as filep:
            for line_number, line in enumerate(filep):
                match = TAG_REGEXP.match(line)
                if match:
                    tag_type_str, tagname, args = match.groups()
                    if not tagname or is_private_identifier(tagname):
                        continue
                    if tagname in self.tags:
                        continue

                    kind = KIND_CLASS if tag_type_str == 'class' else KIND_FUNCTION
                    signature = args.strip()
                    self.tags[tagname] = format_tag(tagname, kind, signature, parent=None)

    def add_builtins(self):
        """
        Add the contents of __builtins__ as simple tags
        """
        builtins = inspect.getmembers(__builtins__)
        for b_name, b_obj in builtins:
            if inspect.isclass(b_obj):
                self._add_tag(b_name, b_obj, KIND_CLASS)
            elif is_relevant_identifier(b_obj):
                self._add_tag(b_name, b_obj, KIND_FUNCTION)

    def write_to_file(self, filename):
        """
        Sort the found tags and write them into the file specified by filename

        @param filename (str)
        """
        write_ctags_file(filename, self.tags.values(), sys.argv[0])


def is_import(object_, module_path):
    object_module = getattr(object_, '__module__', None)
    if object_module and object_module != module_path:
        return True

    return False


def is_private_identifier(tagname):
    return tagname.startswith('_') or tagname.endswith('_')


def is_relevant_identifier(object_):
    # TODO add "inspect.isdatadescriptor" for properties
    # TODO maybe also consider attributes, e.g. by checking against __dict__ or so
    return \
        inspect.ismethod(object_) or \
        inspect.isclass(object_) or \
        inspect.isfunction(object_) or \
        inspect.isgeneratorfunction(object_) or \
        inspect.isgenerator(object_) or \
        inspect.iscoroutinefunction(object_) or \
        inspect.iscoroutine(object_) or \
        inspect.isawaitable(object_) or \
        inspect.isasyncgenfunction(object_) or \
        inspect.isasyncgen(object_) or \
        inspect.isroutine(object_) or \
        inspect.isabstract(object_)


def _setup_global_package_ignore_list():
    """Read the python-config path from LIBPL and strip the prefix part
       (e.g. /usr/lib/python3.8/config-3.8-x86_64-linux-gnu gets config-3.8-x86_64-linux-gnu)
    """
    python_config_dir = Path(sysconfig.get_config_var('LIBPL'))
    try:
        python_config_package = python_config_dir.relative_to(PYTHON_LIB_DIRECTORY)
    except ValueError:
        python_config_package = python_config_dir

    PYTHON_LIB_IGNORE_PACKAGES.append(python_config_package.as_posix())


def _ignore_package(package):
    for ignore in PYTHON_LIB_IGNORE_PACKAGES:
        if ignore in package:
            return True
    return False


def _ignore_module(module):
    return module in PYTHON_LIB_IGNORE_MODULES


def _ignore_class(module, class_):
    return f'{module}.{class_}' in PYTHON_LIB_IGNORE_CLASSES


def _get_module_list(*paths):
    # the loop is quite slow but it doesn't matter for this script
    modules = []
    for path in paths:
        for module_filename in path.rglob('*.py'):
            module_name = module_filename.stem
            package_path = module_filename.relative_to(path)
            package = '.'.join(package_path.parent.parts)
            # construct full module path (e.g. xml.sax.xmlreader)
            if module_name == '__init__':
                module_path = package
            elif package:
                module_path = f'{package}.{module_name}'
            else:
                module_path = module_name

            # ignore unwanted modules and packages
            if _ignore_package(package):
                continue
            if _ignore_module(module_path):
                continue

            modules.append((module_path, module_filename))

    # sort module list for nicer output
    return sorted(modules)


def main():
    _setup_global_package_ignore_list()
    # process modules given on command line
    args = sys.argv[1:]
    if args:
        modules = _get_module_list(*args)
    else:
        modules = _get_module_list(PYTHON_LIB_DIRECTORY)

    parser = Parser()
    parser.add_builtins()

    for module_path, module_filename in modules:
        try:
            parser.process_module(module_path, module_filename)
        except Exception as exc:
            print(f'{exc.__class__.__name__} in {module_path}: {exc}')
            raise

    parser.write_to_file(TAG_FILENAME)


if __name__ == '__main__':
    main()
