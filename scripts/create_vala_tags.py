#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Author:  Fayssal Chamekh
# License: GPL v2 or later
# Adresse: <chamfay(at)gmail(dot)com>
# 
# This script creates geany tags for vala programming language.
# using geany Tagmanager format
#
# Requirements: geany and vala.
#
# By runnig this script without any arguments will generate tags for each
# version of vala
#
# This script also compress generated tags files.
# The default output directory is your home folder.

import subprocess
import os
from os.path import join, exists, basename, splitext

PREFIX = '/usr/share'
DEST_PATH = os.getenv("HOME");

# Get vala installed versions
def get_vala_versions():
    try:
        out = subprocess.check_output('ls -d {}/vala-*'.format(PREFIX), shell=True)
        return out.decode(errors='replace').split()
    except:
        print("Please install vala first.".format(PREFIX))
        return []

# Generate tags from given path.
def generate_tags(vala_path):
    tags_path = join(DEST_PATH, basename(vala_path))
    
    # Create tags folders
    if not exists(tags_path):
        os.mkdir(tags_path)
    
    # Get vapis files
    v_out = subprocess.check_output('ls {}/vapi/*.vapi'.format(vala_path), shell=True);
    v_files = v_out.decode(errors='replace').split()
    
    for v_file in v_files:
        out_prefix = splitext(basename(v_file))[0]
        out_file = join(tags_path, out_prefix + '.vala.tags')
        subprocess.call("geany -g {} {}".format(out_file, v_file), shell=True)
        print("Generate tags for: {}".format(v_file))

# Compress tags
def compress_tags(tags_path):
    os.chdir(DEST_PATH)
    print("Compress files into: {}.tar.bz2 file...".format(tags_path))
    if subprocess.call("tar -cj {0} -f {0}.tar.bz2".format(basename(tags_path)), shell=True) == 0:
        print("Done.")


# Start
for v_path in get_vala_versions():
    generate_tags(v_path)
    tags_path = join(DEST_PATH, basename(v_path))
    compress_tags(tags_path)

