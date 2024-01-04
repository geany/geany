#!/usr/bin/env python3
#
# Author:  The Geany contributors
# License: GPL v2 or later
#
# This is a helper library for Python scripts to create tag files in ctags format,
# e.g. create_py_tags.py and create_php_tags.py.
#

import datetime
import platform


CTAGS_FILE_HEADER = '''!_TAG_FILE_FORMAT	2	/extended format; --format=1 will not append ;" to lines/
!_TAG_FILE_SORTED	1	/0=unsorted, 1=sorted, 2=foldcase/
!_TAG_OUTPUT_EXCMD	mixed	/number, pattern, mixed, or combineV2/
!_TAG_OUTPUT_FILESEP	slash	/slash or backslash/
!_TAG_OUTPUT_MODE	u-ctags	/u-ctags or e-ctags/
!_TAG_PATTERN_LENGTH_LIMIT	96	/0 for no limit/
!_TAG_PROGRAM_NAME {program_name} Automatically generated file - do not edit (created on {timestamp} with Python {python_version})
'''


def format_tag(tagname, kind, signature, parent, return_type=None):
    """
    @param tagname (str)
    @param kind (str)
    @param signature (str)
    @param parent (str)
    @param return_type (tuple of two elements or None)

    """
    if signature:
        signature_field = f'\tsignature:{signature}'
    else:
        signature_field = ''

    if parent:
        parent_field = f'\tclass:{parent}'
    else:
        parent_field = ''

    if return_type:
        return_type_field = f'\ttyperef:{return_type[0]}:{return_type[1]}'
    else:
        return_type_field = ''

    return f'{tagname}\t/unknown\t1;"\tkind:{kind}{parent_field}{signature_field}{return_type_field}\n'


def write_ctags_file(filename, tags, program_name):
    """
    Sort the found tags and write them into the file specified by filename

    @param filename (str)
    @param tags (list)
    @param program_name (str)
    """
    result = sorted(tags)

    ctags_file_header = CTAGS_FILE_HEADER.format(
        program_name=program_name,
        timestamp=datetime.datetime.now().ctime(),
        python_version=platform.python_version())

    with open(filename, 'w', encoding='utf-8') as target_file:
        target_file.write(ctags_file_header)
        for symbol in result:
            if symbol != '\n':  # skip empty lines
                target_file.write(symbol)
