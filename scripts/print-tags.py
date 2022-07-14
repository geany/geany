#!/usr/bin/env python3

import sys

# see tm_source_file.c
TA_NAME, TA_LINE, TA_LOCAL, TA_POS, TA_TYPE, TA_ARGLIST, TA_SCOPE, \
	TA_VARTYPE, TA_INHERITS, TA_TIME, TA_ACCESS, TA_IMPL, TA_LANG, \
	TA_INACTIVE, TA_FLAGS = range(200, 215)

# see TMTagType in tm_parser.h
types = [
	"undef", "class", "enum", "enumerator", "field", "function",
	"interface", "member", "method", "namespace", "package",
	"prototype", "struct", "typedef", "union", "variable", "externvar",
	"macro", "macro_arg", "file", "other",
	"UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN",
]

type_dct = {0: types[0]}
for i in range(len(types) - 1):
	type_dct[1 << i] = types[i + 1]


def decode_kind(kind, value):
	val = ''
	if kind == TA_NAME:
		val = value.decode('utf-8')
	elif kind == TA_LINE:
		val = int(value)
	elif kind == TA_LOCAL:
		val = int(value)
	elif kind == TA_TYPE:
		val = type_dct[int(value)]
	elif kind == TA_ARGLIST:
		val = value.decode('utf-8')
	elif kind == TA_SCOPE:
		val = value.decode('utf-8')
	elif kind == TA_FLAGS:
		val = int(value)
	elif kind == TA_VARTYPE:
		val = value.decode('utf-8')
	elif kind == TA_INHERITS:
		val = value.decode('utf-8')
	elif kind == TA_TIME:
		pass;
	elif kind == TA_LANG:
		pass;
	elif kind == TA_INACTIVE:
		pass;
	elif kind == TA_ACCESS:
		val = value.decode('utf-8')
	elif kind == TA_IMPL:
		val = value.decode('utf-8')
	return val


def print_tag(tag):
	res = '{:<12}'.format(tag[TA_TYPE] + ': ')
	if TA_VARTYPE in tag:
		res += tag[TA_VARTYPE] + ' '
	if TA_SCOPE in tag:
		res += tag[TA_SCOPE] + ' :: '
	res += tag[TA_NAME]
	if TA_ARGLIST in tag:
		res += tag[TA_ARGLIST]
	if TA_INHERITS in tag:
		res += ' extends ' + tag[TA_INHERITS]
	if TA_FLAGS in tag and tag[TA_FLAGS] > 0:
		res += '    flags: ' + str(tag[TA_FLAGS])
	# stdout.buffer (stdin.buffer) needed to write (read) binary data, see:
	# https://docs.python.org/3/library/sys.html?highlight=stdout#sys.stdout
	sys.stdout.buffer.write(res.encode('utf-8'))
	sys.stdout.buffer.write(b'\n')


def get_next_part(inp, pos, first):
	part = b''
	c = inp[pos]
	if first:
		kind = TA_NAME
	else:
		kind = c
		pos += 1
		c = inp[pos]
	while c < TA_NAME and c != ord('\n'):
		part += bytes([c])
		pos += 1
		c = inp[pos]
	return part, kind, pos


inp = sys.stdin.buffer.read()

tag = {}
pos = 0
line_start_pos = 0
line_start = True
first_line = True
while pos < len(inp):
	part, kind, pos = get_next_part(inp, pos, line_start)
	value = decode_kind(kind, part)
	tag[kind] = value
	line_start = False
	if inp[pos] == ord('\n'):
		if first_line:
			pass  # first line contains '# format=tagmanager' we want to skip
		else:
			sys.stdout.buffer.write(inp[line_start_pos:pos])
			sys.stdout.buffer.write(b'\n')
			print_tag(tag)
		first_line = False
		tag = {}
		pos += 1
		line_start_pos = pos
		line_start = True
