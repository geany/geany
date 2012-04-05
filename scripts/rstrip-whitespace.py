#!/usr/bin/env python

import sys

filenames = sys.argv[1:]

def backup_file (fn):
    open ("%s~" % fn, "w").write (open (fn, "r").read ())

for fn in filenames:
  #backup_file (fn)
  contents = [line.rstrip () for line in open (fn)]
  open (fn, "w").write ('\n'.join (contents))
