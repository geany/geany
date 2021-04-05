#!/usr/bin/env python3

import glob
import os
import shutil
import sys

if len(sys.argv) != 3:
    print('Usage: update-ctags.py <universal ctags directory> <geany ctags directory>')

srcdir = os.path.abspath(sys.argv[1])
dstdir = os.path.abspath(sys.argv[2])

os.chdir(dstdir + '/parsers')
parser_dst_files = glob.glob('*.c') + glob.glob('*.h')
parser_dst_files = list(filter(lambda x: not x.startswith('geany_'), parser_dst_files))
os.chdir(srcdir)
print('Copying parsers... ({} files)'.format(len(parser_dst_files)))
for f in parser_dst_files:
    if os.path.exists('parsers/' + f):
        shutil.copy('parsers/' + f, dstdir + '/parsers')
    elif os.path.exists('peg/' + f):
        shutil.copy('peg/' + f, dstdir + '/parsers')
    else:
        print("Error: Could not find file " + f + "!")
        sys.exit(1)

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

os.chdir(dstdir)
os.system('patch -p2 <ctags_changes.patch')
