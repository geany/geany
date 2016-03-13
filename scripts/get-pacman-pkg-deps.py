import sys
import shlex
import collections
import os
import subprocess
import re
from collections import defaultdict
from shlex import split

use_cache = False

def run(args, stdout=subprocess.PIPE):
	com = subprocess.Popen(args, 1024, stdout=stdout, stderr=subprocess.STDOUT, shell=False)
	return com.stdout

class Tree(object):
	def __init__(self, s):
		self.parent = None
		self.childs = []
		self.s = s

	def add(self, s):
		node = Tree(s)
		node.parent = self
		self.childs.append(node)

	def find(self, s):
		if self.s == s:
			return self
		else:
			for node in self.childs:
				n = node.find(s)
				if n:
					return n
			return None

	def has_child(self, s):
		for node in self.childs:
			if node.s == s:
				return True
		return False

	def print_postorder(self, output):
		for node in self.childs:
			node.print_postorder(output)
		if (self.s not in output):
			output.append(self.s)


def parse_tree(infile):
	start = "START"
	startpkg = ""
	root = Tree("")
	p = root
	added = ""
	output = []
	for line in infile.readlines():
		l = split(line)
		if l[0] == 'diagraph':
			start = l[3]
		elif l[0] == start:
			startpkg = l[2].rstrip(';')
			root.s = startpkg
		# special case ['sh', '->', 'bash', '[arrowhead=none,', 'color=grey];']
		# that meabs sh is provided by bash => use bash in the dep tree
		elif len(l) == 5 and l[3].rstrip(',').strip('[') == "arrowhead=none":
			added = l[2]
			p.childs[-1].s = added
		elif len(l) == 4:
			if l[0] == added: # step into tree
				p = p.childs[-1]
			elif l[0] != p.s: # reset parent if node changes
				p = root.find(l[0])
			added = l[2]
			p.add(added)

	root.print_postorder(output)
	return output;

def main(args):
	out = run(['pactree.exe', '-g', args[1]])
	pkgs = parse_tree(out)
	for x in pkgs:
		print(x)

if __name__ == '__main__':
	if (len(sys.argv) < 2):
		sys.stderr.write("ERROR: Package name missing\n")
		exit(1)
	main(sys.argv)
