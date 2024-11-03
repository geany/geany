#!/usr/bin/env python3

import glob
import os
import shutil
import sys

if len(sys.argv) != 3:
    print('Usage: update-ctags.py <universal ctags directory> <geany ctags directory>')
    sys.exit(1)

srcdir = os.path.abspath(sys.argv[1])
dstdir = os.path.abspath(sys.argv[2])

os.chdir(dstdir + '/parsers')
parser_dst_files = glob.glob('*.c') + glob.glob('*.h')
parser_dst_files = list(filter(lambda x: not x.startswith('geany_'), parser_dst_files))
cxx_parser_dst_files = glob.glob('cxx/*.c') + glob.glob('cxx/*.h')
for f in cxx_parser_dst_files:
    os.remove(f)

os.chdir(srcdir + '/parsers')
print('Copying parsers... ({} files)'.format(len(parser_dst_files)))
for f in parser_dst_files:
    shutil.copy(f, dstdir + '/parsers')

cxx_parser_src_files = glob.glob('cxx/*.c') + glob.glob('cxx/*.h')
print('Copying cxx parser files... ({} files)'.format(len(cxx_parser_src_files)))
for f in cxx_parser_src_files:
    shutil.copy(f, dstdir + '/parsers/cxx')

os.chdir(dstdir + '/optlib')
optlib_parser_dst_files = glob.glob('*.c')
os.chdir(srcdir + '/optlib')
print('Copying optlib parsers... ({} files)'.format(len(optlib_parser_dst_files)))
for f in optlib_parser_dst_files:
    shutil.copy(f, dstdir + '/optlib')

print('Copying dsl files...')
for f in ['dsl/es.c', 'dsl/es.h', 'dsl/optscript.c', 'dsl/optscript.h']:
	shutil.copy(srcdir + '/' + f, dstdir + '/' + f)

print('Copying libreadtags files...')
for f in ['libreadtags/readtags.c', 'libreadtags/readtags.h']:
	shutil.copy(srcdir + '/' + f, dstdir + '/' + f)

os.chdir(srcdir)
main_src_files = glob.glob('main/*.c') + glob.glob('main/*.h')
os.chdir(dstdir)
main_dst_files = glob.glob('main/*.c') + glob.glob('main/*.h')

for f in main_dst_files:
    os.remove(f)
os.chdir(srcdir)
print('Copying main... ({} files)'.format(len(main_src_files)))
for f in main_src_files:
    shutil.copy(f, dstdir + '/main')

main_diff = set(main_dst_files) - set(main_src_files)
if main_diff:
    print('Files removed from main: ' + str(main_diff))
main_diff = set(main_src_files) - set(main_dst_files)
if main_diff:
    print('Files added to main: ' + str(main_diff))
