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

import sys, re, os, datetime


# (from tagmanager/tm_tag.c:32)
TA_NAME = '%c' % 200,
TA_TYPE = '%c' % 204
TA_ARGLIST = '%c' % 205

# TMTagType (tagmanager/tm_tag.h:47)
TYPE_CLASS = '%d' % 1
TYPE_FUNCTION = '%d' % 128

tag_filename = 'data/python.tags'
matcher = re.compile('^[ \t]*(def|class)[ \t]+([a-zA-Z0-9_]+)[ \t]*(\(.*\))[:]')


def add_tag(tags, tag):
	end_pos = tag.find(TA_TYPE)
	tagname = tag[0:(end_pos+1)]
	# check for duplicates
	if len(tagname) < 5:
		# skip short tags
		return
	for test in tags:
		if test.startswith(tagname):
			# check whether we find a tag line which starts with the same name,
			# include the separating TA_TYPE character to ensure we don't match
			# writelines() and write()
			return
	tags.append(tag)


def treat_file(tags, filename):
	try:
		fp = open(filename, 'r')
	except:
		sys.stderr.write('Cannot open %s\n' % filename)
		return
	while 1:
		line = fp.readline()
		if not line:
			break
		m = matcher.match(line)
		if m:
			name = m.group(2)
			if name[0] == '_':
				# skip non-public tags
				continue;
			if m.group(1) == 'class':
				type = TYPE_CLASS
			else:
				type = TYPE_FUNCTION
			args = m.group(3).strip()
			# tagnameTA_TYPEtypeTA_ARGLISTarglist\n
			s = name + TA_TYPE + type + TA_ARGLIST + args + '\n'
			add_tag(tags, s)
			#~ # maybe for later use, with a more sophisticated algo to retrieve the API
			#~ scope = ''
			#~ return_value = ''
			#~ # tagnameTA_TYPEtypeTA_ARGLISTarglistTA_SCOPEscopeTA_VARTYPEreturn_value\n
			#~ s = name + TA_TYPE + type + TA_ARGLIST + args + TA_SCOPE + scope + TA_VARTYPE + return_value + '\n'



# files to include if none were specified on command line
# (this list was created manually and probably needs review for sensible input files)
default_files = [
'/usr/lib/python2.5/anydbm.py',
'/usr/lib/python2.5/asynchat.py',
'/usr/lib/python2.5/asyncore.py',
'/usr/lib/python2.5/audiodev.py',
'/usr/lib/python2.5/base64.py',
'/usr/lib/python2.5/BaseHTTPServer.py',
'/usr/lib/python2.5/Bastion.py',
'/usr/lib/python2.5/bdb.py',
'/usr/lib/python2.5/binhex.py',
'/usr/lib/python2.5/bisect.py',
'/usr/lib/python2.5/calendar.py',
'/usr/lib/python2.5/CGIHTTPServer.py',
'/usr/lib/python2.5/cgi.py',
'/usr/lib/python2.5/cgitb.py',
'/usr/lib/python2.5/chunk.py',
'/usr/lib/python2.5/cmd.py',
'/usr/lib/python2.5/codecs.py',
'/usr/lib/python2.5/codeop.py',
'/usr/lib/python2.5/code.py',
'/usr/lib/python2.5/colorsys.py',
'/usr/lib/python2.5/commands.py',
'/usr/lib/python2.5/compileall.py',
'/usr/lib/python2.5/ConfigParser.py',
'/usr/lib/python2.5/contextlib.py',
'/usr/lib/python2.5/cookielib.py',
'/usr/lib/python2.5/Cookie.py',
'/usr/lib/python2.5/copy.py',
'/usr/lib/python2.5/copy_reg.py',
'/usr/lib/python2.5/cProfile.py',
'/usr/lib/python2.5/csv.py',
'/usr/lib/python2.5/dbhash.py',
'/usr/lib/python2.5/decimal.py',
'/usr/lib/python2.5/difflib.py',
'/usr/lib/python2.5/dircache.py',
'/usr/lib/python2.5/dis.py',
'/usr/lib/python2.5/DocXMLRPCServer.py',
'/usr/lib/python2.5/filecmp.py',
'/usr/lib/python2.5/fileinput.py',
'/usr/lib/python2.5/fnmatch.py',
'/usr/lib/python2.5/formatter.py',
'/usr/lib/python2.5/fpformat.py',
'/usr/lib/python2.5/ftplib.py',
'/usr/lib/python2.5/functools.py',
'/usr/lib/python2.5/getopt.py',
'/usr/lib/python2.5/getpass.py',
'/usr/lib/python2.5/gettext.py',
'/usr/lib/python2.5/glob.py',
'/usr/lib/python2.5/gopherlib.py',
'/usr/lib/python2.5/gzip.py',
'/usr/lib/python2.5/hashlib.py',
'/usr/lib/python2.5/heapq.py',
'/usr/lib/python2.5/hmac.py',
'/usr/lib/python2.5/htmlentitydefs.py',
'/usr/lib/python2.5/htmllib.py',
'/usr/lib/python2.5/HTMLParser.py',
'/usr/lib/python2.5/httplib.py',
'/usr/lib/python2.5/ihooks.py',
'/usr/lib/python2.5/imaplib.py',
'/usr/lib/python2.5/imghdr.py',
'/usr/lib/python2.5/imputil.py',
'/usr/lib/python2.5/inspect.py',
'/usr/lib/python2.5/keyword.py',
'/usr/lib/python2.5/linecache.py',
'/usr/lib/python2.5/locale.py',
'/usr/lib/python2.5/mailbox.py',
'/usr/lib/python2.5/mailcap.py',
'/usr/lib/python2.5/markupbase.py',
'/usr/lib/python2.5/md5.py',
'/usr/lib/python2.5/mhlib.py',
'/usr/lib/python2.5/mimetools.py',
'/usr/lib/python2.5/mimetypes.py',
'/usr/lib/python2.5/MimeWriter.py',
'/usr/lib/python2.5/mimify.py',
'/usr/lib/python2.5/modulefinder.py',
'/usr/lib/python2.5/multifile.py',
'/usr/lib/python2.5/mutex.py',
'/usr/lib/python2.5/netrc.py',
'/usr/lib/python2.5/nntplib.py',
'/usr/lib/python2.5/ntpath.py',
'/usr/lib/python2.5/nturl2path.py',
'/usr/lib/python2.5/opcode.py',
'/usr/lib/python2.5/optparse.py',
'/usr/lib/python2.5/os2emxpath.py',
'/usr/lib/python2.5/os.py',
'/usr/lib/python2.5/pdb.py',
'/usr/lib/python2.5/pickle.py',
'/usr/lib/python2.5/pickletools.py',
'/usr/lib/python2.5/pipes.py',
'/usr/lib/python2.5/pkgutil.py',
'/usr/lib/python2.5/platform.py',
'/usr/lib/python2.5/plistlib.py',
'/usr/lib/python2.5/popen2.py',
'/usr/lib/python2.5/poplib.py',
'/usr/lib/python2.5/posixfile.py',
'/usr/lib/python2.5/posixpath.py',
'/usr/lib/python2.5/pprint.py',
'/usr/lib/python2.5/pty.py',
'/usr/lib/python2.5/py_compile.py',
'/usr/lib/python2.5/pydoc.py',
'/usr/lib/python2.5/Queue.py',
'/usr/lib/python2.5/quopri.py',
'/usr/lib/python2.5/random.py',
'/usr/lib/python2.5/repr.py',
'/usr/lib/python2.5/re.py',
'/usr/lib/python2.5/rexec.py',
'/usr/lib/python2.5/rfc822.py',
'/usr/lib/python2.5/rlcompleter.py',
'/usr/lib/python2.5/robotparser.py',
'/usr/lib/python2.5/runpy.py',
'/usr/lib/python2.5/sched.py',
'/usr/lib/python2.5/sets.py',
'/usr/lib/python2.5/sha.py',
'/usr/lib/python2.5/shelve.py',
'/usr/lib/python2.5/shlex.py',
'/usr/lib/python2.5/shutil.py',
'/usr/lib/python2.5/SimpleHTTPServer.py',
'/usr/lib/python2.5/SimpleXMLRPCServer.py',
'/usr/lib/python2.5/site.py',
'/usr/lib/python2.5/smtpd.py',
'/usr/lib/python2.5/smtplib.py',
'/usr/lib/python2.5/sndhdr.py',
'/usr/lib/python2.5/socket.py',
'/usr/lib/python2.5/SocketServer.py',
'/usr/lib/python2.5/stat.py',
'/usr/lib/python2.5/statvfs.py',
'/usr/lib/python2.5/StringIO.py',
'/usr/lib/python2.5/stringold.py',
'/usr/lib/python2.5/stringprep.py',
'/usr/lib/python2.5/string.py',
'/usr/lib/python2.5/_strptime.py',
'/usr/lib/python2.5/struct.py',
'/usr/lib/python2.5/subprocess.py',
'/usr/lib/python2.5/sunaudio.py',
'/usr/lib/python2.5/sunau.py',
'/usr/lib/python2.5/symbol.py',
'/usr/lib/python2.5/symtable.py',
'/usr/lib/python2.5/tabnanny.py',
'/usr/lib/python2.5/tarfile.py',
'/usr/lib/python2.5/telnetlib.py',
'/usr/lib/python2.5/tempfile.py',
'/usr/lib/python2.5/textwrap.py',
'/usr/lib/python2.5/this.py',
'/usr/lib/python2.5/threading.py',
'/usr/lib/python2.5/timeit.py',
'/usr/lib/python2.5/toaiff.py',
'/usr/lib/python2.5/tokenize.py',
'/usr/lib/python2.5/token.py',
'/usr/lib/python2.5/traceback.py',
'/usr/lib/python2.5/trace.py',
'/usr/lib/python2.5/tty.py',
'/usr/lib/python2.5/types.py',
'/usr/lib/python2.5/unittest.py',
'/usr/lib/python2.5/urllib2.py',
'/usr/lib/python2.5/urllib.py',
'/usr/lib/python2.5/urlparse.py',
'/usr/lib/python2.5/UserDict.py',
'/usr/lib/python2.5/UserList.py',
'/usr/lib/python2.5/user.py',
'/usr/lib/python2.5/UserString.py',
'/usr/lib/python2.5/uuid.py',
'/usr/lib/python2.5/uu.py',
'/usr/lib/python2.5/warnings.py',
'/usr/lib/python2.5/wave.py',
'/usr/lib/python2.5/weakref.py',
'/usr/lib/python2.5/webbrowser.py',
'/usr/lib/python2.5/whichdb.py',
'/usr/lib/python2.5/xdrlib.py',
'/usr/lib/python2.5/zipfile.py' ]



def main():
	tags = []

	# process files given on command line
	args = sys.argv[1:]
	if not args:
		args = default_files

	for filename in args:
		treat_file(tags, filename)

	# sort the tags
	tags.sort()
	# write them
	fp = open(tag_filename, 'wb')
	fp.write(
		'# format=tagmanager - Automatically generated file - do not edit (created on %s)\n' % \
		datetime.datetime.now().ctime())
	for s in tags:
		if not s == '\n': # skip empty lines
			fp.write(s)
	fp.close()


if __name__ == '__main__':
	main()

