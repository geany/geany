#!/usr/bin/env python

import sys

filenames = sys.argv[1:]

def backup_file (fn):
    open ("%s~" % fn, "w").write (open (fn, "r").read ())

for fn in filenames:
  #backup_file (fn)
  contents = open (fn, "r").read ()
  lines = contents.split ('\n')
  with open (fn, "w") as fobj:
    for line in lines:
      line = line.rstrip ()
      fobj.write ("%s\n" % line)
  contents = open (fn, "r").read ()
  contents.rstrip ()
  while contents[-1] in " \t\r\n":
    contents = contents[:-1]
  open (fn, "w").write ("%s\n" % contents)

