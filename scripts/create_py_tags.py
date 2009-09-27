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


class Parser:

	#----------------------------------------------------------------------
	def __init__(self):
		self.tags = []

	#----------------------------------------------------------------------
	def add_tag(self, tag):
		"""
		Verify the found tag name and if it is valid, add it to the list

		@param tag (str)
		"""
		end_pos = tag.find(TA_TYPE)
		tagname = tag[0:(end_pos+1)]
		# check for duplicates
		if len(tagname) < 5:
			# skip short tags
			return
		for test in self.tags:
			if test.startswith(tagname):
				# check whether we find a tag line which starts with the same name,
				# include the separating TA_TYPE character to ensure we don't match
				# writelines() and write()
				return
		self.tags.append(tag)

	#----------------------------------------------------------------------
	def process_file(self, filename):
		"""
		Read the file specified by filename and look for class and function definitions

		@param filename (str)
		"""
		try:
			fp = open(filename, 'r')
		except:
			sys.stderr.write('Cannot open %s\n' % filename)
			return
		for line in fp:
			m = matcher.match(line)
			if m:
				tag_type_str, name, args = m.groups()
				if not name or name[0] == '_':
					# skip non-public tags
					continue;
				if tag_type_str == 'class':
					tag_type = TYPE_CLASS
				else:
					tag_type = TYPE_FUNCTION
				args = args.strip()
				# tagnameTA_TYPEtypeTA_ARGLISTarglist\n
				s = name + TA_TYPE + tag_type + TA_ARGLIST + args + '\n'
				self.add_tag(s)
				#~ # maybe for later use, with a more sophisticated algo to retrieve the API
				#~ scope = ''
				#~ return_value = ''
				#~ # tagnameTA_TYPEtypeTA_ARGLISTarglistTA_SCOPEscopeTA_VARTYPEreturn_value\n
				#~ s = name + TA_TYPE + type + TA_ARGLIST + args + TA_SCOPE + scope + TA_VARTYPE + return_value + '\n'

	#----------------------------------------------------------------------
	def tags_to_file(self, filename):
		"""
		Sort the found tags and write them into the file specified by filename

		@param filename (str)
		"""
		# sort the tags
		self.tags.sort()
		# write them
		fp = open(filename, 'wb')
		fp.write(
			'# format=tagmanager - Automatically generated file - do not edit (created on %s)\n' % \
			datetime.datetime.now().ctime())
		for s in self.tags:
			if not s == '\n': # skip empty lines
				fp.write(s)
		fp.close()



# files to include if none were specified on command line
# (this list was created manually and probably needs review for sensible input files)
default_files = map(lambda x: '/usr/lib/python2.5/' + x,
[ 'anydbm.py', 'asynchat.py', 'asyncore.py', 'audiodev.py', 'base64.py', 'BaseHTTPServer.py',
'Bastion.py', 'bdb.py', 'binhex.py', 'bisect.py', 'calendar.py', 'CGIHTTPServer.py',
'cgi.py', 'cgitb.py', 'chunk.py', 'cmd.py', 'codecs.py', 'codeop.py', 'code.py', 'colorsys.py',
'commands.py', 'compileall.py', 'ConfigParser.py', 'contextlib.py', 'cookielib.py', 'Cookie.py',
'copy.py', 'copy_reg.py', 'cProfile.py', 'csv.py', 'dbhash.py', 'decimal.py', 'difflib.py',
'dircache.py', 'dis.py', 'DocXMLRPCServer.py', 'filecmp.py', 'fileinput.py', 'fnmatch.py',
'formatter.py', 'fpformat.py', 'ftplib.py', 'functools.py', 'getopt.py', 'getpass.py', 'gettext.py',
'glob.py', 'gopherlib.py', 'gzip.py', 'hashlib.py', 'heapq.py', 'hmac.py', 'htmlentitydefs.py',
'htmllib.py', 'HTMLParser.py', 'httplib.py', 'ihooks.py', 'imaplib.py', 'imghdr.py', 'imputil.py',
'inspect.py', 'keyword.py', 'linecache.py', 'locale.py', 'mailbox.py', 'mailcap.py', 'markupbase.py',
'md5.py', 'mhlib.py', 'mimetools.py', 'mimetypes.py', 'MimeWriter.py', 'mimify.py',
'modulefinder.py', 'multifile.py', 'mutex.py', 'netrc.py', 'nntplib.py', 'ntpath.py',
'nturl2path.py', 'opcode.py', 'optparse.py', 'os2emxpath.py', 'os.py', 'pdb.py', 'pickle.py',
'pickletools.py', 'pipes.py', 'pkgutil.py', 'platform.py', 'plistlib.py', 'popen2.py',
'poplib.py', 'posixfile.py', 'posixpath.py', 'pprint.py', 'pty.py', 'py_compile.py', 'pydoc.py',
'Queue.py', 'quopri.py', 'random.py', 'repr.py', 're.py', 'rexec.py', 'rfc822.py', 'rlcompleter.py',
'robotparser.py', 'runpy.py', 'sched.py', 'sets.py', 'sha.py', 'shelve.py', 'shlex.py', 'shutil.py',
'SimpleHTTPServer.py', 'SimpleXMLRPCServer.py', 'site.py', 'smtpd.py', 'smtplib.py', 'sndhdr.py',
'socket.py', 'SocketServer.py', 'stat.py', 'statvfs.py', 'StringIO.py', 'stringold.py',
'stringprep.py', 'string.py', '_strptime.py', 'struct.py', 'subprocess.py', 'sunaudio.py',
'sunau.py', 'symbol.py', 'symtable.py', 'tabnanny.py', 'tarfile.py', 'telnetlib.py', 'tempfile.py',
'textwrap.py', 'this.py', 'threading.py', 'timeit.py', 'toaiff.py', 'tokenize.py', 'token.py',
'traceback.py', 'trace.py', 'tty.py', 'types.py', 'unittest.py', 'urllib2.py', 'urllib.py',
'urlparse.py', 'UserDict.py', 'UserList.py', 'user.py', 'UserString.py', 'uuid.py', 'uu.py',
'warnings.py', 'wave.py', 'weakref.py', 'webbrowser.py', 'whichdb.py', 'xdrlib.py', 'zipfile.py'
])


def main():
	# process files given on command line
	args = sys.argv[1:]
	if not args:
		args = default_files

	parser = Parser()

	for filename in args:
		parser.process_file(filename)

	parser.tags_to_file(tag_filename)


if __name__ == '__main__':
	main()

