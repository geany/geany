#!/usr/bin/env python3
#
# Author:  Enrico Tröger
# License: GPL v2 or later
#
# This script downloads the PHP tag definitions in JSON format from
# http://doc.php.net/downloads/json/php_manual_en.json.
# From those defintions all function tags are extracted and written
# to ../data/tags/std.php.tags (relative to the script's location, not $CWD).

import re
from json import loads
from os.path import dirname, join
from urllib.request import urlopen


UPSTREAM_TAG_DEFINITION = 'http://doc.php.net/downloads/json/php_manual_en.json'
PROTOTYPE_RE = r'^(?P<return_type>.*) {tag_name}(?P<arg_list>\(.*\))$'

# (from src/tagmanager/tm_tag.c:90)
TA_NAME = 200
TA_TYPE = 204
TA_ARGLIST = 205
TA_SCOPE = 206
TA_VARTYPE = 207
# TMTagType (src/tagmanager/tm_tag.h:49)
TYPE_CLASS = 1
TYPE_FUNCTION = 16
TYPE_MEMBER = 64
TYPE_METHOD = 128
TYPE_VARIABLE = 16384


def normalize_name(name):
    """ Replace namespace separator with class separators, as Geany only
        understands the latter """
    return name.replace('\\', '::')


def split_scope(name):
    """ Splits the scope from the member, and returns (scope, member).
        Returned scope is None if the name is not a member """
    name = normalize_name(name)
    sep_pos = name.rfind('::')
    if sep_pos < 0:
        return None, name
    else:
        return name[:sep_pos], name[sep_pos + 2:]


def parse_and_create_php_tags_file():
    # download upstream definition
    response = urlopen(UPSTREAM_TAG_DEFINITION)
    try:
        html = response.read()
    finally:
        response.close()

    # parse JSON
    definitions = loads(html)

    # generate tags
    tag_list = []
    for tag_name, tag_definition in definitions.items():
        prototype_re = PROTOTYPE_RE.format(tag_name=re.escape(tag_name))
        match = re.match(prototype_re, tag_definition['prototype'])
        if match:
            return_type = normalize_name(match.group('return_type'))
            arg_list = match.group('arg_list')

            scope, tag_name = split_scope(tag_name)
            if tag_name[0] == '$':
                tag_type = TYPE_MEMBER if scope is not None else TYPE_VARIABLE
            else:
                tag_type = TYPE_METHOD if scope is not None else TYPE_FUNCTION
            tag_list.append((tag_name, tag_type, return_type, arg_list, scope))
            # Also create a class tag when encountering a __construct()
            if tag_name == '__construct' and scope is not None:
                scope, tag_name = split_scope(scope)
                tag_list.append((tag_name, TYPE_CLASS, None, arg_list, scope or ''))

    # write tags
    script_dir = dirname(__file__)
    tags_file_path = join(script_dir, '..', 'data', 'tags', 'std.php.tags')
    with open(tags_file_path, 'w', encoding='iso-8859-1') as tags_file:
        tags_file.write('# format=tagmanager\n')
        for tag_name, tag_type, return_type, arg_list, scope in sorted(tag_list):
            tag_line = f'{tag_name}'
            for attr, type_ in [(tag_type, TA_TYPE),
                               (arg_list, TA_ARGLIST),
                               (return_type, TA_VARTYPE),
                               (scope, TA_SCOPE)]:
                if attr is not None:
                    tag_line += f'{type_:c}{attr}'

            tags_file.write(tag_line + '\n')
    print(f'Created: {tags_file_path} with {len(tag_list)} tags')


if __name__ == '__main__':
    parse_and_create_php_tags_file()
