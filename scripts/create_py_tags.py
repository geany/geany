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

import datetime
import imp
import inspect
import os
import re
import string
import sys
import types


# (from tagmanager/tm_tag.c:32)
TA_NAME = '%c' % 200,
TA_TYPE = '%c' % 204
TA_ARGLIST = '%c' % 205
TA_SCOPE = '%c' % 206

# TMTagType (tagmanager/tm_tag.h:47)
TYPE_CLASS = '%d' % 1
TYPE_FUNCTION = '%d' % 128

tag_filename = 'data/python.tags'
tag_regexp = '^[ \t]*(def|class)[ \t]+([a-zA-Z0-9_]+)[ \t]*(\(.*\))[:]'


class Parser:

	#----------------------------------------------------------------------
	def __init__(self):
		self.tags = {}
		self.re_matcher = re.compile(tag_regexp)

	#----------------------------------------------------------------------
	def _get_superclass(self, c):
		"""
		Python class base-finder
		(found on http://mail.python.org/pipermail/python-list/2002-November/173949.html)

		@param c (object)
		@return superclass (object)
		"""
		try:
			#~ TODO print inspect.getmro(c)
			if type(c) == types.ClassType:
				return c.__bases__[0].__name__
			else:
				return c.__mro__[1].__name__
		except Exception, e:
			return ''

	#----------------------------------------------------------------------
	def _formatargspec(self, args, varargs=None, varkw=None, defaults=None,
					  formatarg=str,
					  formatvarargs=lambda name: '*' + name,
					  formatvarkw=lambda name: '**' + name,
					  formatvalue=lambda value: '=' + repr(value),
					  join=inspect.joinseq):
		"""Format an argument spec from the 4 values returned by getargspec.

		The first four arguments are (args, varargs, varkw, defaults).  The
		other four arguments are the corresponding optional formatting functions
		that are called to turn names and values into strings.  The ninth
		argument is an optional function to format the sequence of arguments."""
		specs = []
		if defaults:
			firstdefault = len(args) - len(defaults)
		for i in range(len(args)):
			spec = inspect.strseq(args[i], formatarg, join)
			if defaults and i >= firstdefault:
				d = defaults[i - firstdefault]
				# this is the difference from the original formatargspec() function
				# to use nicer names then the default repr() output
				if hasattr(d, '__name__'):
					d = d.__name__
				spec = spec + formatvalue(d)
			specs.append(spec)
		if varargs is not None:
			specs.append(formatvarargs(varargs))
		if varkw is not None:
			specs.append(formatvarkw(varkw))
		return '(' + string.join(specs, ', ') + ')'

	#----------------------------------------------------------------------
	def _add_tag(self, obj, tag_type, parent=''):
		"""
		Verify the found tag name and if it is valid, add it to the list

		@param obj (instance)
		@param tag_type (str)
		@param parent (str)
		"""
		args = ''
		scope = ''
		try:
			args = apply(self._formatargspec, inspect.getargspec(obj))
		except TypeError, KeyError:
			pass
		if parent:
			if tag_type == TYPE_CLASS:
				args = '(%s)' % parent
			else:
				scope = '%s%s' % (TA_SCOPE, parent)
		tagname = obj.__name__
		# check for duplicates
		if len(tagname) < 4:
			# skip short tags
			return
		tag = '%s%s%s%s%s%s\n' % (tagname, TA_TYPE, tag_type, TA_ARGLIST, args, scope)
		if not tagname in self.tags:
			self.tags[tagname] = tag

	#----------------------------------------------------------------------
	def process_file(self, filename):
		"""
		Read the file specified by filename and look for class and function definitions

		@param filename (str)
		"""
		try:
			module = imp.load_source('tags_file_module', filename)
		except Exception, e:
			module = None

		if module:
			symbols = inspect.getmembers(module, callable)
			for obj_name, obj in symbols:
				try:
					name = obj.__name__
				except AttributeError:
					name = obj_name
				if not name or name.startswith('_'):
					# skip non-public tags
					continue;
				if inspect.isfunction(obj):
					self._add_tag(obj, TYPE_FUNCTION)
				elif inspect.isclass(obj):
					self._add_tag(obj, TYPE_CLASS, self._get_superclass(obj))
					methods = inspect.getmembers(obj, inspect.ismethod)
					for m_name, m_obj in methods:
						# skip non-public tags
						if m_name.startswith('_') or not inspect.ismethod(m_obj):
							continue
						self._add_tag(m_obj, TYPE_FUNCTION, name)
		else:
			# plain regular expression based parsing
			fp = open(filename)
			for line in fp:
				m = self.re_matcher.match(line)
				if m:
					tag_type_str, tagname, args = m.groups()
					if not tagname or tagname.startswith('_'):
						# skip non-public tags
						continue;
					if tag_type_str == 'class':
						tag_type = TYPE_CLASS
					else:
						tag_type = TYPE_FUNCTION
					args = args.strip()
					tag = '%s%s%s%s%s\n' % (tagname, TA_TYPE, tag_type, TA_ARGLIST, args)
					if not tagname in self.tags:
						self.tags[tagname] = tag
			fp.close()

	#----------------------------------------------------------------------
	def write_to_file(self, filename):
		"""
		Sort the found tags and write them into the file specified by filename

		@param filename (str)
		"""
		result = self.tags.values()
		# sort the tags
		result.sort()
		# write them
		fp = open(filename, 'wb')
		fp.write(
			'# format=tagmanager - Automatically generated file - do not edit (created on %s)\n' % \
			datetime.datetime.now().ctime())
		for s in result:
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

	parser.write_to_file(tag_filename)


if __name__ == '__main__':
	main()

